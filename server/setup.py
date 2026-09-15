# -*- coding: utf-8 -*-
from setuptools import setup, Command
from setuptools.command.install import install as _install
from setuptools.command.develop import develop as _develop
from distutils.command.build import build as _build
import subprocess
import shutil
import os
import re
import sys
import platform

IS_WINDOWS = platform.system() == 'Windows'
IS_MAC = platform.system() == 'Darwin'

if sys.version_info < (3, 5):
    sys.exit("Python version must be >= 3.5")


def read(*paths):
    this_dir = os.path.dirname(os.path.realpath(__file__))
    content = open(os.path.join(this_dir, *paths), "rb").read()
    return content.decode("utf-8")


version = re.search("__version__ = '(.+)'", read('funq_server/__init__.py')).group(1)


class build_libfunq(Command):
    """
    Construction de la lib c++.
    """
    user_options = [
        ('build-lib=', 'd', "directory to \"build\" (copy) to"),
        ('force', 'f', "forcibly build everything (ignore file timestamps)"),
        ('cmake-path=', None, "path to the cmake executable"),
        ('make-path=', None, "path to the make executable"),
        ('debug', 'g',
         "compile/link with debugging information (default: OFF)"),
        ('inplace', 'i',
         "ignore build-lib and put compiled extensions into the source " +
         "directory alongside your pure Python modules"),
    ]

    boolean_options = ['inplace', 'debug', 'force']

    def initialize_options(self):
        self.build_lib = None
        self.force = None
        self.inplace = None
        self.debug = None  # ✅ 修改：从 True 改为 None，表示未设置
        self.cmake_path = None
        self.make_path = None
        self.qt_version = None

        if IS_WINDOWS:
            self.funqlib_name = 'libFunq.dll'
        elif IS_MAC:
            self.funqlib_name = 'libFunq.dylib'
        else:
            self.funqlib_name = 'libFunq.so'

    def finalize_options(self):
        self.set_undefined_options('build',
                                   ('force', 'force'),
                                   ('debug', 'debug'),
                                   ('build_lib', 'build_lib'))
        
        # ✅ 新增：Debug 模式灵活切换逻辑
        # 优先级：命令行参数 > 环境变量 > 默认(False)
        if self.debug is None:
            # 检查环境变量
            env_debug = os.environ.get('FUNQ_DEBUG')
            if env_debug:
                # 支持多种环境变量值：1, true, True, yes, Yes
                self.debug = env_debug.lower() in ('1', 'true', 'yes')
            else:
                # 默认使用 Release 版本（生产环境友好）
                self.debug = False
        
        # ✅ 打印构建类型提示
        build_type = 'Debug' if self.debug else 'Release'
        print(f'Building {build_type} version...')
        
        if self.cmake_path is None:
            self.cmake_path = os.environ.get('FUNQ_CMAKE_PATH') or 'cmake'
        if self.make_path is None:
            self.make_path = os.environ.get('FUNQ_MAKE_PATH') or 'make'
        if self.qt_version is None:
            self.qt_version = os.environ.get('FUNQ_QT_MAJOR_VERSION')

    def funqlib_out_path(self):
        funqlib_base_dir = '.' if self.inplace else self.build_lib
        return os.path.join(funqlib_base_dir, 'funq_server', self.funqlib_name)

    def run(self):
        # 根据构建类型设置路径
        buildtype = 'Debug' if self.debug else 'Release'
        
        # 使用动态路径
        src_path = os.path.join('libFunq', buildtype, self.funqlib_name)
        if os.path.exists(src_path) and not self.force:
            print('Using pre-built library: %s' % src_path)
        else:
            if self.force:
                # Windows 用 cmake 清理，其他用 make
                if IS_WINDOWS:
                    subprocess.call([self.cmake_path, '--build', '.', '--config', buildtype, '--clean-first'], shell=True)
                else:
                    subprocess.call([self.make_path, 'clean'], shell=True)
            
            cmake_cmd = [
                self.cmake_path, '.',
                '-DCMAKE_BUILD_TYPE={}'.format(buildtype),
            ]
            
            # Windows 上指定 Visual Studio 生成器和架构
            if IS_WINDOWS:
                cmake_cmd.extend(['-G', 'Visual Studio 17 2022', '-A', 'x64'])
            
            if self.qt_version is not None:
                cmake_cmd += ['-DQT_MAJOR_VERSION={}'.format(self.qt_version)]
            print('running %s' % cmake_cmd)
            subprocess.check_call(cmake_cmd)

            # Windows 用 cmake --build，其他用 make
            if IS_WINDOWS:
                make_cmd = [self.cmake_path, '--build', '.', '--config', buildtype]
            else:
                make_cmd = [self.make_path]
            print('running %s' % make_cmd)
            subprocess.check_call(make_cmd, shell=True)

        lib_path = self.funqlib_out_path()
        lib_dir = os.path.dirname(lib_path)
        if not os.path.isdir(lib_dir):
            os.makedirs(lib_dir)
        
        # 使用动态路径复制
        if IS_WINDOWS:
            src_path = os.path.join('libFunq', buildtype, self.funqlib_name)
        else:
            src_path = os.path.join('libFunq', self.funqlib_name)
        
        if os.path.exists(src_path):
            shutil.copy2(src_path, lib_path)
            print('Copied %s to %s' % (src_path, lib_path))
        else:
            raise FileNotFoundError('Could not find built library at: %s' % src_path)

    def get_outputs(self):
        return [self.funqlib_out_path()]


class build(_build):
    sub_commands = _build.sub_commands + [('build_libfunq', None)]


class install(_install):
    def run(self):
        self.run_command('build_libfunq')
        _install.run(self)


class develop(_develop):
    def run(self):
        self.reinitialize_command('build_libfunq', inplace=1)
        self.run_command('build_libfunq')
        _develop.run(self)


setup(
    name='funq-server',
    author="Julien Pagès",
    author_email="j.parkouss@gmail.com",
    url="https://github.com/parkouss/funq",
    description="write FUNctional tests for Qt applications (server)",
    long_description=read("README"),
    version=version,
    packages=['funq_server'],
    entry_points={
        'console_scripts': [
            'funq = funq_server.runner:main'
        ]
    },
    cmdclass={
        'build_libfunq': build_libfunq,
        'build': build,
        'install': install,
        'develop': develop,
    },
    install_requires=[],
    license='CeCILL v1.2',
)
