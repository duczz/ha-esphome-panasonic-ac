"""Tests the CNT command retries against a fake AC on a pseudo terminal.

Needs a venv with ESPHome (aioesphomeapi comes with it) on Linux or macOS:

    python tests/test_cnt_retry.py
"""

import asyncio
import contextlib
import logging
import os
from pathlib import Path
import pty
import re
import subprocess
import sys
import time
import tty

from aioesphomeapi import APIClient, ClimateMode, LogLevel

CONFIG = Path(__file__).parent / "cnt_retry.yaml"
BINARY = Path(__file__).parent / ".esphome/build/cnt-retry-test/.pioenvs/cnt-retry-test/program"
CONTROLLED = (0, 1, 2, 3, 4, 5, 8)
#        heat  22.0  -     fan 3 swing normal -     -     eco   -
START = [0x44, 0x2C, 0x80, 0x50, 0x36, 0x00, 0x00, 0x40, 0x00, 0x00]


def frame(header, payload):
    data = bytes([header, len(payload), *payload])
    return data + bytes([-sum(data) & 0xFF])


class FakeAC:
    """Answers polls after 50 ms and ignores the next `lose` control frames and `lose_polls` polls."""

    def __init__(self):
        self.fd, slave = pty.openpty()
        tty.setraw(slave)
        self.port = os.ttyname(slave)
        self.state = list(START)
        self.lose = 0
        self.lose_on = ()
        self.lose_polls = 0
        self.reply_to_commands = False
        self.commands = []  # (time, payload)
        self.polls = []
        self.applied = None  # Called after a control frame was handled
        self.poll_delay = 0.05
        self.buffer = b""
        asyncio.get_running_loop().add_reader(self.fd, self.read)

    def read(self):
        self.buffer += os.read(self.fd, 256)
        while len(self.buffer) >= 13:
            packet, self.buffer = self.buffer[:13], self.buffer[13:]
            assert sum(packet) & 0xFF == 0, packet.hex()
            if packet[0] == 0x70:
                self.polls.append(time.monotonic())
                if self.lose_polls > 0:
                    self.lose_polls -= 1
                    continue
                # A poll carries the state from the time it was received
                self.respond()
            else:
                assert packet[0] == 0xF0
                self.commands.append((time.monotonic(), list(packet[2:12])))
                if self.lose > 0 or len(self.commands) in self.lose_on:
                    self.lose = max(0, self.lose - 1)
                else:
                    for i in CONTROLLED:
                        self.state[i] = packet[2 + i]
                    if self.reply_to_commands:
                        self.respond()
                if self.applied:
                    self.applied(len(self.commands))

    def respond(self):
        response = frame(0x70, self.state + [0x00] * 6 + [22, 5, 0xFF] + [0x80] * 13)
        asyncio.get_running_loop().call_later(self.poll_delay, os.write, self.fd, response)


class Device:
    async def start(self):
        self.ac = FakeAC()
        self.process = subprocess.Popen(
            [BINARY], env={**os.environ, "AC_PORT": self.ac.port}, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT
        )
        self.client = APIClient("127.0.0.1", 16053, None)
        for _ in range(50):
            with contextlib.suppress(Exception):
                await self.client.connect()
                break
            await asyncio.sleep(0.1)
        self.logs = []
        self.client.subscribe_logs(
            lambda m: self.logs.append(re.sub(r"\x1b\[[\d;]*m", "", m.message.decode())), LogLevel.LOG_LEVEL_VERBOSE
        )
        entities = (await self.client.list_entities_services())[0]
        self.keys = {e.name: e.key for e in entities}
        self.states = {}
        self.client.subscribe_states(lambda s: self.states.__setitem__(s.key, s))
        while not self.ac.polls:
            await asyncio.sleep(0.05)
        await asyncio.sleep(0.5)
        return self

    async def stop(self):
        self.process.kill()
        self.process.wait()
        asyncio.get_running_loop().remove_reader(self.ac.fd)
        os.close(self.ac.fd)

    def fan(self, mode):
        self.client.climate_command(self.keys["AC"], custom_fan_mode=mode)

    def temperature(self, value):
        self.client.climate_command(self.keys["AC"], target_temperature=value)

    def switch(self, name, state):
        self.client.switch_command(self.keys[name], state)

    def count(self, text):
        return sum(text in line for line in self.logs)

    def check_gaps(self):
        frames = sorted([t for t, _ in self.ac.commands] + self.ac.polls)
        assert all(b - a > 0.24 for a, b in zip(frames, frames[1:])), frames


async def confirmed_without_resend(d):
    d.fan("4")
    await asyncio.sleep(2.5)
    assert d.ac.state[3] == 0x60
    assert len(d.ac.commands) == 1
    # One extra poll about 1 s after the command, then the normal cadence again
    assert len(d.ac.polls) == 2, d.ac.polls
    assert 0.9 < d.ac.polls[1] - d.ac.commands[0][0] < 1.3
    assert d.count("resend") == 0
    await asyncio.sleep(4.5)
    assert len(d.ac.polls) == 3
    assert 4.9 < d.ac.polls[2] - d.ac.polls[1] < 5.3


async def lost_once(d):
    d.ac.lose = 1
    d.fan("4")
    await asyncio.sleep(3.5)
    assert d.ac.state[3] == 0x60
    assert len(d.ac.commands) == 2
    assert d.ac.commands[0][1] == d.ac.commands[1][1]
    assert d.ac.commands[1][0] - d.ac.commands[0][0] < 1.7
    assert d.count("resending (1/5)") == 1
    assert d.count("Command confirmed after 1 resend(s)") == 1
    assert d.states[d.keys["AC"]].custom_fan_mode == "4"


async def confirm_polls_lost(d):
    d.ac.lose = 1
    d.ac.lose_polls = 2
    d.fan("4")
    await asyncio.sleep(5.5)
    assert d.ac.state[3] == 0x60
    assert len(d.ac.commands) == 2
    assert d.ac.commands[1][0] - d.ac.commands[0][0] < 3.8
    assert d.count("Command confirmed after 1 resend(s)") == 1


async def silent_ac(d):
    polls = len(d.ac.polls)
    d.ac.lose = 1
    d.ac.lose_polls = 1000
    d.fan("4")
    await asyncio.sleep(13)
    # 5 fast polls, then back to the regular interval, and no blind resend
    assert 6 <= len(d.ac.polls) - polls <= 8
    assert len(d.ac.commands) == 1
    d.ac.state[0] = 0x34  # IR remote while the AC does not answer
    d.ac.lose_polls = 0
    await asyncio.sleep(7)
    assert len(d.ac.commands) == 1
    assert d.count("changed externally") == 1


async def ac_replies_to_commands(d):
    d.ac.reply_to_commands = True
    d.ac.lose = 1
    d.fan("4")
    await asyncio.sleep(4)
    assert d.ac.state[3] == 0x60
    assert len(d.ac.commands) == 2
    assert d.count("changed externally") == 0


async def reply_confirms_before_remote_revert(d):
    def remote(_):
        # IR remote: back to the old state, after the AC answered the command but before the confirming poll
        asyncio.get_running_loop().call_later(0.4, d.ac.state.__setitem__, 3, 0x50)

    d.ac.reply_to_commands = True
    d.ac.applied = remote
    d.fan("4")
    await asyncio.sleep(8)
    assert len(d.ac.commands) == 1
    assert d.ac.state[3] == 0x50
    assert d.count("resending") == 0


async def revert_with_late_poll_reply(d):
    d.ac.poll_delay = 1.1
    d.ac.lose_on = {2}
    polls = len(d.ac.polls)
    while len(d.ac.polls) == polls:
        await asyncio.sleep(0.01)
    # The answer to this poll shows the old state, which is the requested one again after the second command
    d.fan("4")
    await asyncio.sleep(0.45)
    d.fan("3")
    await asyncio.sleep(9)
    assert [c[3] for _, c in d.ac.commands[:2]] == [0x60, 0x50]
    assert d.ac.state[3] == 0x50


async def fan_after_lost_preset(d):
    d.ac.lose = 1
    d.client.climate_command(d.keys["AC"], custom_preset="Powerful")
    await asyncio.sleep(0.3)
    d.fan("4")
    await asyncio.sleep(3)
    assert d.ac.state[3] == 0x60
    assert d.ac.state[5] & 0x0F == 0


async def eco_and_mild_dry_lost(d):
    for name, index, value in (("Eco", 8, 0x40), ("Mild Dry", 2, 0x7F)):
        d.ac.lose = 1
        d.switch(name, True)
        await asyncio.sleep(3)
        assert d.ac.state[index] == value
        assert d.states[d.keys[name]].state is True
    assert len(d.ac.commands) == 4
    assert d.count("Command confirmed after 1 resend(s)") == 2


async def gives_up(d):
    d.ac.lose = 1000
    d.fan("4")
    await asyncio.sleep(15)
    assert len(d.ac.commands) == 6  # The command and 5 resends
    assert d.count("giving up") == 1
    assert d.states[d.keys["AC"]].custom_fan_mode == "3"
    # The next command works again and does not bring the old one back
    d.ac.lose = 0
    d.temperature(24)
    await asyncio.sleep(2)
    assert d.ac.state[1] == 48 and d.ac.state[3] == 0x50
    assert len(d.ac.commands) == 7


async def external_change(d):
    def remote(_):
        d.ac.state[0] = 0x34  # IR remote: cool

    d.ac.lose = 1
    d.ac.applied = remote
    d.fan("4")
    await asyncio.sleep(8)
    assert len(d.ac.commands) == 1
    assert d.count("changed externally") == 1
    assert d.ac.state[0] == 0x34 and d.ac.state[3] == 0x50
    # A new command starts from the state set by the remote
    d.ac.applied = None
    d.temperature(24)
    await asyncio.sleep(2)
    assert d.ac.state[:4] == [0x34, 48, 0x80, 0x50]


async def second_change_while_pending(d):
    d.ac.lose = 1
    d.fan("4")
    await asyncio.sleep(0.5)
    d.temperature(24)
    await asyncio.sleep(3)
    assert len(d.ac.commands) == 2
    assert d.ac.commands[1][1][1] == 48 and d.ac.commands[1][1][3] == 0x60
    assert d.ac.state[1] == 48 and d.ac.state[3] == 0x60


async def both_changes_lost(d):
    d.ac.lose = 2
    d.fan("4")
    await asyncio.sleep(0.5)
    d.temperature(24)
    await asyncio.sleep(4)
    assert len(d.ac.commands) == 3
    assert d.ac.state[1] == 48 and d.ac.state[3] == 0x60


async def first_applied_second_lost(d):
    def lose_second(count):
        d.ac.lose = 1 if count == 1 else 0

    d.ac.applied = lose_second
    d.fan("4")
    await asyncio.sleep(0.5)
    d.temperature(24)
    await asyncio.sleep(4)
    assert len(d.ac.commands) == 3
    assert d.ac.state[1] == 48 and d.ac.state[3] == 0x60
    assert d.count("changed externally") == 0


async def revert_while_pending(d):
    # The AC never sees "on", the user turns it off again after the first poll
    d.ac.lose = 1
    d.switch("Eco", True)
    await asyncio.sleep(1.15)
    d.switch("Eco", False)
    await asyncio.sleep(4)
    assert d.ac.state[8] == 0x00
    assert d.ac.commands[-1][1][8] == 0x00
    assert d.states[d.keys["Eco"]].state is False


async def revert_select_while_pending(d):
    d.client.select_command(d.keys["Vertical"], "up")
    await asyncio.sleep(0.5)
    d.client.select_command(d.keys["Vertical"], "center")
    await asyncio.sleep(3)
    assert [c[1][4] for c in d.ac.commands] == [0x16, 0x36]
    assert d.ac.state[4] == 0x36


async def poll_sent_before_command(d):
    # The response to the last poll before the command arrives after it
    d.ac.poll_delay = 0.6
    while len(d.ac.polls) < 2:
        await asyncio.sleep(0.01)
    d.fan("4")
    await asyncio.sleep(0.3)
    d.ac.poll_delay = 0.05
    await asyncio.sleep(3)
    assert 0.2 < d.ac.commands[0][0] - d.ac.polls[1] < 0.5
    assert len(d.ac.commands) == 1
    assert d.count("resending") == 0


async def off_is_retried(d):
    d.ac.lose = 2
    d.client.climate_command(d.keys["AC"], mode=ClimateMode.OFF)
    await asyncio.sleep(5)
    assert d.ac.state[0] == 0x40
    assert len(d.ac.commands) == 3


async def retries_and_delay_options(d):
    d.ac.lose = 1000
    d.fan("4")
    await asyncio.sleep(5)
    assert len(d.ac.commands) == 3
    assert d.count("resending (2/2)") == 1
    assert d.count("giving up") == 1
    assert 0.45 < d.ac.polls[1] - d.ac.commands[0][0] < 0.8


async def disabled(d):
    d.ac.lose = 1
    d.fan("4")
    await asyncio.sleep(0.5)
    # Built from the last reported state, so it undoes the fan change
    d.temperature(24)
    await asyncio.sleep(6.5)
    expected = list(START)
    expected[3] = 0x60
    assert d.ac.commands[0][1] == expected
    expected = list(START)
    expected[1] = 48
    assert [c[1] for c in d.ac.commands[1:]] == [expected]
    # No extra poll, the next one is 5 s after the last command
    assert len(d.ac.polls) == 2
    assert 4.9 < d.ac.polls[1] - d.ac.commands[1][0] < 5.3
    assert d.count("resend") == 0


SUITES = [
    (
        {},
        [
            confirmed_without_resend,
            lost_once,
            confirm_polls_lost,
            silent_ac,
            ac_replies_to_commands,
            reply_confirms_before_remote_revert,
            revert_with_late_poll_reply,
            fan_after_lost_preset,
            eco_and_mild_dry_lost,
            gives_up,
            external_change,
            second_change_while_pending,
            both_changes_lost,
            first_applied_second_lost,
            revert_while_pending,
            revert_select_while_pending,
            poll_sent_before_command,
            off_is_retried,
        ],
    ),
    ({"command_retries": "2", "command_confirm_delay": "500ms"}, [retries_and_delay_options]),
    ({"command_retries": "0"}, [disabled]),
]


async def run(test):
    device = await Device().start()
    try:
        await test(device)
        device.check_gaps()
    except AssertionError:
        print("\n".join(device.logs))
        raise
    finally:
        await device.stop()


def main():
    logging.getLogger("aioesphomeapi").setLevel(logging.CRITICAL)
    for substitutions, tests in SUITES:
        args = [a for item in substitutions.items() for a in ("-s", *item)]
        subprocess.run(
            [sys.executable, "-m", "esphome", "-q", *args, "compile", CONFIG], check=True, stdout=subprocess.DEVNULL
        )
        for test in tests:
            asyncio.run(run(test))
            print(f"PASS {test.__name__} {substitutions or ''}", flush=True)


if __name__ == "__main__":
    main()
