"""
retrace -- Python config builder + CLI wrapper for retrace v2.

Generates JSON configs programmatically and invokes the retrace
CLI. No C extension needed -- pure Python 3, works anywhere.

Quick start:

    from retrace import Config, Action, Script

    # Build a config that traces HTTP traffic
    config = Config()
    config.trace_http()
    config.trace_dns()
    config.save("trace.json")

    # Run a command under retrace
    config.run("/usr/bin/curl https://example.com")

Or use the fluent API:

    config = Config()
    config.add("open") \
          .log_params() \
          .call_real()
    config.add("malloc") \
          .log_params() \
          .call_real() \
          .memory_fuzz(fail_rate=0.05)
    config.filter("open", param="flags", op="==", value=0)
    config.save("debug.json")

Convenience presets:

    Config.log_all()         -- log_params + call_real for "*"
    Config.fail_mallocs(5)   -- first 5 mallocs pass, rest fail
    Config.sandbox_paths(["/etc/shadow"])
    Config.trace_http()
    Config.trace_dns()

The generated JSON is valid retrace config format (see
docs/configuration.md for the schema).
"""

import json
import os
import subprocess
import tempfile
from typing import Any, Optional, Union, List, Dict


class Action:
    """A single action in an intercept script."""

    def __init__(self, name: str, **params: Any):
        self.name = name
        self.params = params

    def to_dict(self) -> Dict:
        d: Dict[str, Any] = {"action_name": self.name}
        if self.params:
            d["action_params"] = self.params
        return d

    def __repr__(self) -> str:
        if self.params:
            return f"Action({self.name!r}, **{self.params!r})"
        return f"Action({self.name!r})"


class Script:
    """An intercept script for one function."""

    def __init__(self, func_name: str,
                 actions: Optional[List[Action]] = None,
                 caller_matches: Optional[List[Dict]] = None,
                 return_addr: Optional[int] = None):
        self.func_name = func_name
        self.actions: List[Action] = actions or []
        self.caller_matches = caller_matches
        self.return_addr = return_addr

    def add(self, action: Union[Action, str], **params: Any) -> "Script":
        if isinstance(action, str):
            action = Action(action, **params)
        self.actions.append(action)
        return self

    # Fluent action builders
    def log_params(self, **params: Any) -> "Script":
        return self.add(Action("log_params", **params))

    def call_real(self) -> "Script":
        return self.add(Action("call_real"))

    def modify_return(self, value: int) -> "Script":
        return self.add(Action("modify_return_value_int",
                               retval_int=value))

    def memory_fuzz(self, fail_rate: float = 0.1) -> "Script":
        return self.add(Action("memory_fuzz", fail_rate=fail_rate))

    def delay(self, ms: int) -> "Script":
        return self.add(Action("delay", ms=ms))

    def count_limit(self, limit: int) -> "Script":
        return self.add(Action("call_count_limit", limit=limit))

    def sandbox(self, deny_paths: List[str]) -> "Script":
        return self.add(Action("sandbox", deny_paths=deny_paths))

    def addr_deny(self, deny_addrs: List[str]) -> "Script":
        return self.add(Action("addr_deny", deny_addrs=deny_addrs))

    def filter(self, param_name: str, op: str, value: int) -> "Script":
        return self.add(Action("filter", param_name=param_name,
                               op=op, value=value))

    def decode_http(self, param_name: str = "buf") -> "Script":
        return self.add(Action("decode_http", param_name=param_name))

    def decode_dns(self, param_name: str = "buf") -> "Script":
        return self.add(Action("decode_dns", param_name=param_name))

    def incomplete_io(self, rate: float) -> "Script":
        return self.add(Action("incomplete_io", rate=rate))

    def fuzzing_seed(self, seed: int) -> "Script":
        return self.add(Action("fuzzing_seed", seed=seed))

    def modify_param_int(self, param_name: str, new_int: int,
                         match_int: Optional[int] = None) -> "Script":
        params: Dict[str, Any] = {"param_name": param_name,
                                  "new_int": new_int}
        if match_int is not None:
            params["match_int"] = match_int
        return self.add(Action("modify_in_param_int", **params))

    def modify_param_str(self, param_name: str, new_str: str,
                         match_str: Optional[str] = None) -> "Script":
        params: Dict[str, Any] = {"param_name": param_name,
                                  "new_str": new_str}
        if match_str is not None:
            params["match_str"] = match_str
        return self.add(Action("modify_in_param_str", **params))

    def to_dict(self) -> Dict:
        d: Dict[str, Any] = {"func_name": self.func_name}
        if self.caller_matches:
            d["caller_matches"] = self.caller_matches
        if self.return_addr:
            d["return_addr"] = self.return_addr
        d["actions"] = [a.to_dict() for a in self.actions]
        return d


class Config:
    """A complete retrace JSON configuration."""

    def __init__(self):
        self.scripts: List[Script] = []

    def add(self, func_name: str, **kwargs: Any) -> Script:
        s = Script(func_name, **kwargs)
        self.scripts.append(s)
        return s

    def filter(self, func_name: str, param: str = None,
               param_name: str = None, op: str = "==",
               value: int = 0) -> Script:
        """Add a filter guard to a function."""
        pn = param_name or param
        return self.add(func_name).filter(pn, op, value)

    # --- Presets ---

    @classmethod
    def log_all(cls) -> "Config":
        """Log all calls with default config (wildcard match)."""
        c = cls()
        c.add("*").log_params().call_real()
        return c

    @classmethod
    def fail_mallocs(cls, limit: int = 5) -> "Config":
        """Let first N mallocs succeed, then fail the rest."""
        c = cls()
        c.add("malloc") \
         .modify_return(0) \
         .count_limit(limit)
        return c

    @classmethod
    def sandbox_paths(cls, deny_paths: List[str]) -> "Config":
        """Deny access to specific file paths."""
        c = cls()
        c.add("open").sandbox(deny_paths)
        c.add("openat").sandbox(deny_paths)
        c.add("fopen").sandbox(deny_paths)
        return c

    @classmethod
    def trace_http(cls) -> "Config":
        """Trace HTTP request/response data via send/recv."""
        c = cls()
        c.add("send").decode_http().log_params().call_real()
        c.add("sendto").decode_http().log_params().call_real()
        s = c.add("recv")
        s.call_real().decode_http().log_params()
        s2 = c.add("recvfrom")
        s2.call_real().decode_http().log_params()
        return c

    @classmethod
    def trace_dns(cls) -> "Config":
        """Trace DNS queries via sendto."""
        c = cls()
        c.add("sendto").decode_dns().log_params().call_real()
        return c

    @classmethod
    def trace_network(cls) -> "Config":
        """Trace all network activity (HTTP + DNS + raw sockets)."""
        c = cls()
        c.trace_http()
        c.trace_dns()
        for fn in ["socket", "connect", "bind", "listen", "accept"]:
            c.add(fn).log_params().call_real()
        return c

    # --- Serialization ---

    def to_dict(self) -> Dict:
        return {"intercept_scripts": [s.to_dict() for s in self.scripts]}

    def to_json(self, indent: int = 2) -> str:
        return json.dumps(self.to_dict(), indent=indent)

    def save(self, path: str) -> None:
        with open(path, "w") as f:
            f.write(self.to_json())

    # --- Execution ---

    def run(self, command: Union[str, List[str]],
            retrace_path: str = "retrace",
            env: Optional[Dict[str, str]] = None,
            capture_output: bool = False) -> subprocess.CompletedProcess:
        """Run a command under retrace with this config."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".json",
                                          delete=False) as f:
            f.write(self.to_json())
            config_path = f.name

        try:
            full_env = os.environ.copy()
            full_env["RETRACE_JSON_CONFIG"] = config_path
            if env:
                full_env.update(env)

            cmd = [retrace_path, "run", "--"]
            if isinstance(command, str):
                cmd.append(command)
            else:
                cmd.extend(command)

            return subprocess.run(cmd, env=full_env,
                                  capture_output=capture_output)
        finally:
            os.unlink(config_path)

    def __repr__(self) -> str:
        return f"Config({len(self.scripts)} scripts)"
