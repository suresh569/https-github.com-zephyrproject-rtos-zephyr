# Copyright (c) 2023 Christophe Dufaza <chris@openmarl.org>
#
# SPDX-License-Identifier: Apache-2.0

"""Helpers for DTSh unit tests."""


from typing import Generator, Dict, Optional

from pathlib import Path
import os
import contextlib

from devicetree import edtlib

from dtsh.model import DTModel


class DTShTests:
    """Access test resource files, data samples, etc."""

    ZEPHYR_BASE = os.environ.get("ZEPHYR_BASE") or str(
        Path(__file__).parent.parent.parent.parent.parent
    )
    """A ZEPHYR_BASE test runners may explicitly set."""

    RES_BASE = os.path.join(Path(__file__).parent.parent, "tests", "res")
    """Unit tests resource files directory."""

    ANON_ZEPHYR_BASE = "/path/to/zephyr_base"
    """Anonymized ZEPHYR_BASE that appears in CMake cache files."""

    ANON_ZEPHYR_SDK = "/path/to/zephyr-sdk"
    """Anonymized Zephyr SDK path that appears in CMake cache files."""

    ANON_GNUARMEMB = "/path/to/gnuarmemb"
    """Anonymized Arm toolchain that path appears in CMake cache files."""

    ZEPHYR_VERSION = "3.4.99"
    """Zephyr project version that appears in CMake cache files."""

    BOARD = "nrf52840dk_nrf52840"
    """The unit tests board model."""

    _sample_edt: Optional[edtlib.EDT] = None
    _sample_dtmodel: Optional[DTModel] = None

    @classmethod
    @contextlib.contextmanager
    def mock_env(
        cls, tmpenv: Dict[str, Optional[str]]
    ) -> Generator[None, None, None]:
        """Temporarily change the OS environment.

           None values will unset the variables.

            with mock_env({"ZEPHYR_BASE": None, "NAME": "VALUE"}):
                # Unit test for API behaviors that depend on environment variables.

        Args:
            tmpenv: The OS environment changes.
        """
        # Save the environment subset the mock will change or unset the values of.
        diffenv = {name: os.environ.get(name) for name in tmpenv}
        for name, value in tmpenv.items():
            if value is not None:
                # Change or add environment values.
                os.environ[name] = value
            elif name in os.environ:
                # Unset defined environment variables.
                del os.environ[name]

        try:
            # Work with temp OS environment.
            yield

        finally:
            for name, value in diffenv.items():
                if value is not None:
                    # Restore the environment subset the mock had changed
                    # the values of.
                    os.environ[name] = value
                elif name in os.environ:
                    # Unset the environment subset the mock added.
                    del os.environ[name]

    @classmethod
    @contextlib.contextmanager
    def from_res(cls) -> Generator[None, None, None]:
        """Temporarily change the working directory to the resources location.

        Implementation idea borrowed from the "from_here()"  pattern
        in Zephyr's python-devicetree (test_edtlib.py).
        """
        cwd = os.getcwd()
        try:
            os.chdir(cls.RES_BASE)
            yield
        finally:
            os.chdir(cwd)

    @classmethod
    @contextlib.contextmanager
    def from_there(cls, *name: str) -> Generator[None, None, None]:
        """Temporarily change the working directory."""
        cwd = os.getcwd()
        try:
            os.chdir(os.path.join(cls.RES_BASE, *name))
            yield
        finally:
            os.chdir(cwd)

    @classmethod
    def get_resource_path(cls, *name: str) -> str:
        """Translate path name components to an absolute resource file path.

        Args:
            *name: Path components.

        Returns:
            The absolute path to the resource file.
        """
        return os.path.join(cls.RES_BASE, *name)

    @classmethod
    def get_sample_edt(cls, force_reload: bool = False) -> edtlib.EDT:
        """Get the sample Devicetree model (EDT).

        Args:
            force_reload: Force reloading the model (default is to return
              a cached model).

        Returns:
            The sample Devicetree model (EDT).
        """
        if force_reload:
            return cls._read_sample_edt()
        if not cls._sample_edt:
            cls._sample_edt = cls._read_sample_edt()
        return cls._sample_edt

    @classmethod
    def get_sample_dtmodel(cls, force_reload: bool = False) -> DTModel:
        """Get the sample Devicetree model (DTModel).

        Args:
            force_reload: Force reloading the model (default is to return
              a cached model).

        Returns:
            The sample Devicetree model (DTModel).
        """
        if force_reload:
            return cls._read_sample_dtmodel()
        if not cls._sample_dtmodel:
            cls._sample_dtmodel = cls._read_sample_dtmodel()
        return cls._sample_dtmodel

    @classmethod
    def _read_sample_edt(cls) -> edtlib.EDT:
        with cls.from_res():
            return edtlib.EDT(
                dts="zephyr.dts",
                bindings_dirs=[
                    os.path.join(cls.ZEPHYR_BASE, "dts", "bindings")
                ],
            )

    @classmethod
    def _read_sample_dtmodel(cls) -> DTModel:
        with cls.from_res():
            return DTModel.create(
                dts_path="zephyr.dts",
                binding_dirs=[os.path.join(cls.ZEPHYR_BASE, "dts", "bindings")],
            )
