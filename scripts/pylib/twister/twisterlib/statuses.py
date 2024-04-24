#!/usr/bin/env python3
# Copyright (c) 2024 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0
"""
Status classes to be used instead of str statuses.
"""

from enum import Enum


# Status Assignment tree:
#   parent <- child <=> parent is assigned to child in code.
#
#   HarnessStatus
#   └ QEMUOutputStatus
#     └ TestInstanceStatus
#       ├ TestSuiteStatus
#       └ TestCaseStatus


class TestInstanceStatus(str, Enum):
    def __str__(self):
        return str(self.value)

    # Values assigned directly
    NONE = None  # to preserve old functionality
    ERROR = 'error'
    FAIL = 'failed'
    FILTER = 'filtered'
    PASS = 'passed'
    SKIP = 'skipped'


class TestSuiteStatus(str, Enum):
    def __str__(self):
        return str(self.value)

    # Values assigned directly
    NONE = None  # to preserve old functionality
    FILTER = 'filtered'
    PASS = 'passed'
    SKIP = 'skipped'

    # Values assigned via TestInstanceStatus
    ERROR = 'error'
    FAIL = 'failed'


class TestCaseStatus(str, Enum):
    def __str__(self):
        return str(self.value)

    # Values assigned directly
    NONE = None  # to preserve old functionality
    BLOCK = 'blocked'
    ERROR = 'error'
    FAIL = 'failed'
    FILTER = 'filtered'
    PASS = 'passed'
    SKIP = 'skipped'
    STARTED = 'started'


class QEMUOutputStatus(str, Enum):
    def __str__(self):
        return str(self.value)

    # Values assigned directly
    NONE = None  # to preserve old functionality
    BYTE = 'unexpected byte'
    EOF = 'unexpected eof'
    FAIL = 'failed'
    TIMEOUT = 'timeout'

    # Values assigned via HarnessStatus
    ERROR = 'error'
    PASS = 'passed'
    SKIP = 'skipped'


class HarnessStatus(str, Enum):
    def __str__(self):
        return str(self.value)

    # Values assigned directly
    NONE = None  # to preserve old functionality
    ERROR = 'error'
    FAIL = 'failed'
    PASS = 'passed'
    SKIP = 'skipped'
