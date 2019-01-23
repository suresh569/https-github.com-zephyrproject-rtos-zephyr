# Copyright (c) 2018 Foundries.io
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import os

from west import log
from west import cmake
from west.build import DEFAULT_BUILD_DIR, DEFAULT_CMAKE_GENERATOR, \
    is_zephyr_build
from west.commands import WestCommand

BUILD_DESCRIPTION = '''\
Convenience wrapper for building Zephyr applications.

This command attempts to do what you mean when run from a Zephyr
application source or a pre-existing build directory:

- When "west build" is run from a Zephyr build directory, the source
  directory is obtained from the CMake cache, and that build directory
  is re-compiled.

- Otherwise, the source directory defaults to the current working
  directory, so running "west build" from a Zephyr application's
  source directory compiles it.

The source and build directories can be explicitly set with the
--source-dir and --build-dir options. The build directory defaults to
'build' if it is not auto-detected. The build directory is always
created if it does not exist.

This command runs CMake to generate a build system if one is not
present in the build directory, then builds the application.
Subsequent builds try to avoid re-running CMake; you can force it
to run by setting --cmake.

To pass additional options to CMake, give them as extra arguments
after a '--' For example, "west build -- -DOVERLAY_CONFIG=some.conf" sets
an overlay config file. (Doing this forces a CMake run.)'''


class Build(WestCommand):

    def __init__(self):
        super(Build, self).__init__(
            'build',
            'compile a Zephyr application',
            BUILD_DESCRIPTION,
            accepts_unknown_args=False)

        self.source_dir = None
        '''Source directory for the build, or None on error.'''

        self.build_dir = None
        '''Final build directory used to run the build, or None on error.'''

        self.created_build_dir = False
        '''True if the build directory was created; False otherwise.'''

        self.run_cmake = False
        '''True if CMake was run; False otherwise.

        Note: this only describes CMake runs done by this command. The
        build system generated by CMake may also update itself due to
        internal logic.'''

        self.cmake_cache = None
        '''Final parsed CMake cache for the build, or None on error.'''

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            help=self.help,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            description=self.description)

        # Remember to update scripts/west-completion.bash if you add or remove
        # flags

        parser.add_argument('-b', '--board',
                            help='''Board to build for (must be given for the
                            first build, can be omitted later)''')
        parser.add_argument('-s', '--source-dir',
                            help='''Explicitly set the source directory.
                            If not given and rebuilding an existing Zephyr
                            build directory, this is taken from the CMake
                            cache. Otherwise, the current directory is
                            assumed.''')
        parser.add_argument('-d', '--build-dir',
                            help='''Explicitly sets the build directory.
                            If not given and the current directory is a Zephyr
                            build directory, it will be used; otherwise, "{}"
                            is assumed. The directory will be created if
                            it doesn't exist.'''.format(DEFAULT_BUILD_DIR))
        parser.add_argument('-t', '--target',
                            help='''Override the build system target (e.g.
                            'clean', 'pristine', etc.)''')
        parser.add_argument('-c', '--cmake', action='store_true',
                            help='Force CMake to run')
        parser.add_argument('-f', '--force', action='store_true',
                            help='Ignore any errors and try to build anyway')
        parser.add_argument('cmake_opts', nargs='*', metavar='cmake_opt',
                            help='Extra option to pass to CMake; implies -c')

        return parser

    def do_run(self, args, ignored):
        self.args = args        # Avoid having to pass them around
        log.dbg('args:', args, level=log.VERBOSE_EXTREME)
        self._sanity_precheck()
        self._setup_build_dir()
        if is_zephyr_build(self.build_dir):
            self._update_cache()
            if self.args.cmake or self.args.cmake_opts:
                self.run_cmake = True
        else:
            self.run_cmake = True
        self._setup_source_dir()
        self._sanity_check()

        log.inf('source directory: {}'.format(self.source_dir), colorize=True)
        log.inf('build directory: {}{}'.
                format(self.build_dir,
                       (' (created)' if self.created_build_dir
                        else '')),
                colorize=True)
        if self.cmake_cache:
            board = self.cmake_cache.get('CACHED_BOARD')
        elif self.args.board:
            board = self.args.board
        else:
            board = 'UNKNOWN'   # shouldn't happen
        log.inf('BOARD:', board, colorize=True)

        self._run_cmake(self.args.cmake_opts)
        self._sanity_check()
        self._update_cache()

        extra_args = ['--target', args.target] if args.target else []
        cmake.run_build(self.build_dir, extra_args=extra_args)

    def _sanity_precheck(self):
        app = self.args.source_dir
        if app:
            if not os.path.isdir(app):
                self._check_force('source directory {} does not exist'.
                                  format(app))
            elif 'CMakeLists.txt' not in os.listdir(app):
                self._check_force("{} doesn't contain a CMakeLists.txt".
                                  format(app))

    def _update_cache(self):
        try:
            self.cmake_cache = cmake.CMakeCache.from_build_dir(self.build_dir)
        except FileNotFoundError:
            pass

    def _setup_build_dir(self):
        # Initialize build_dir and created_build_dir attributes.
        log.dbg('setting up build directory', level=log.VERBOSE_EXTREME)
        if self.args.build_dir:
            build_dir = self.args.build_dir
        else:
            cwd = os.getcwd()
            if is_zephyr_build(cwd):
                build_dir = cwd
            else:
                build_dir = DEFAULT_BUILD_DIR
        build_dir = os.path.abspath(build_dir)

        if os.path.exists(build_dir):
            if not os.path.isdir(build_dir):
                log.die('build directory {} exists and is not a directory'.
                        format(build_dir))
        else:
            os.makedirs(build_dir, exist_ok=False)
            self.created_build_dir = True
            self.run_cmake = True

        self.build_dir = build_dir

    def _setup_source_dir(self):
        # Initialize source_dir attribute, either from command line argument,
        # implicitly from the build directory's CMake cache, or using the
        # default (current working directory).
        log.dbg('setting up source directory', level=log.VERBOSE_EXTREME)
        if self.args.source_dir:
            source_dir = self.args.source_dir
        elif self.cmake_cache:
            source_dir = self.cmake_cache.get('APPLICATION_SOURCE_DIR')
            if not source_dir:
                # Maybe Zephyr changed the key? Give the user a way
                # to retry, at least.
                log.die("can't determine application from build directory "
                        "{}, please specify an application to build".
                        format(self.build_dir))
        else:
            source_dir = os.getcwd()
        self.source_dir = os.path.abspath(source_dir)

    def _sanity_check(self):
        # Sanity check the build configuration.
        # Side effect: may update cmake_cache attribute.
        log.dbg('sanity checking the build', level=log.VERBOSE_EXTREME)
        if self.source_dir == self.build_dir:
            # There's no forcing this.
            log.die('source and build directory {} cannot be the same; '
                    'use --build-dir {} to specify a build directory'.
                    format(self.source_dir, self.build_dir))

        srcrel = os.path.relpath(self.source_dir)
        if is_zephyr_build(self.source_dir):
            self._check_force('it looks like {srcrel} is a build directory: '
                              'did you mean -build-dir {srcrel} instead?'.
                              format(srcrel=srcrel))
        elif 'CMakeLists.txt' not in os.listdir(self.source_dir):
            self._check_force('source directory "{srcrel}" does not contain '
                              'a CMakeLists.txt; is that really what you '
                              'want to build? (Use -s SOURCE_DIR to specify '
                              'the application source directory)'.
                              format(srcrel=srcrel))

        if not is_zephyr_build(self.build_dir) and not self.args.board:
            self._check_force('this looks like a new or clean build, '
                              'please provide --board')

        if not self.cmake_cache:
            return          # That's all we can check without a cache.

        cached_app = self.cmake_cache.get('APPLICATION_SOURCE_DIR')
        log.dbg('APPLICATION_SOURCE_DIR:', cached_app,
                level=log.VERBOSE_EXTREME)
        source_abs = (os.path.abspath(self.args.source_dir)
                      if self.args.source_dir else None)
        cached_abs = os.path.abspath(cached_app) if cached_app else None
        if cached_abs and source_abs and source_abs != cached_abs:
            self._check_force('build directory "{}" is for application "{}", '
                              'but source directory "{}" was specified; '
                              'please clean it or use --build-dir to set '
                              'another build directory'.
                              format(self.build_dir, cached_abs,
                                     source_abs))
            self.run_cmake = True  # If they insist, we need to re-run cmake.

        cached_board = self.cmake_cache.get('CACHED_BOARD')
        log.dbg('CACHED_BOARD:', cached_board, level=log.VERBOSE_EXTREME)
        if not cached_board and not self.args.board:
            if self.created_build_dir:
                self._check_force(
                    'Building for the first time: you must provide --board')
            else:
                self._check_force(
                    'Board is missing or unknown, please provide --board')
        if self.args.board and cached_board and \
           self.args.board != cached_board:
            self._check_force('Build directory {} targets board {}, '
                              'but board {} was specified. (Clean that '
                              'directory or use --build-dir to specify '
                              'a different one.)'.
                              format(self.build_dir, cached_board,
                                     self.args.board))

    def _check_force(self, msg):
        if not self.args.force:
            log.err(msg)
            log.die('refusing to proceed without --force due to above error')

    def _run_cmake(self, cmake_opts):
        if not self.run_cmake:
            log.dbg('not running cmake; build system is present')
            return

        # It's unfortunate to have to use the undocumented -B and -H
        # options to set the source and binary directories.
        #
        # However, it's the only known way to set that directory and
        # run CMake from the current working directory. This is
        # important because users expect invocations like this to Just
        # Work:
        #
        # west build -- -DOVERLAY_CONFIG=relative-path.conf
        final_cmake_args = ['-B{}'.format(self.build_dir),
                            '-H{}'.format(self.source_dir),
                            '-G{}'.format(DEFAULT_CMAKE_GENERATOR)]
        if self.args.board:
            final_cmake_args.append('-DBOARD={}'.format(self.args.board))
        if cmake_opts:
            final_cmake_args.extend(cmake_opts)
        cmake.run_cmake(final_cmake_args)
