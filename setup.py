import shutil
import os
import glob
import subprocess
import sys
import site
import sysconfig
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
from setuptools.command.install_lib import install_lib

library = None
PYTHON = sys.executable

# Default project name
project_name = 'essentia'

var_project_name = 'ESSENTIA_PROJECT_NAME'
if var_project_name in os.environ:
    project_name = os.environ[var_project_name]


class EssentiaInstall(install_lib):
    def install(self):
        global library
        install_dir = os.path.join(self.install_dir, library.split(os.sep)[-1])
        res = shutil.move(library, install_dir)
        os.system("ls -l %s" % self.install_dir)
        return [install_dir]


class EssentiaBuildExtension(build_ext):
    def run(self):
        global library
        os.system('rm -rf tmp; mkdir tmp')

        # Ugly hack using an enviroment variable... There's no way to pass a
        # custom flag to python setup.py bdist_wheel
        var_skip_3rdparty = 'ESSENTIA_WHEEL_SKIP_3RDPARTY'
        var_only_python = 'ESSENTIA_WHEEL_ONLY_PYTHON'

        var_macos_arm64 = os.getenv('ESSENTIA_MACOSX_ARM64')
        macos_arm64_flags = []
        if var_macos_arm64 == '1':
            # waf expects option/value as separate argv entries for --arch.
            macos_arm64_flags = ['--arch', 'arm64', '--no-msse']
        windows_flags = []
        static_deps_flags = ['--static-dependencies']
        build_static_flags = ['--build-static']
        if sys.platform.startswith('win'):
            # Avoid linking prepackaged x86 static deps on Windows wheels and
            # minimize optional dependencies that often miss x64 import libs.
            windows_flags = ['--fft=KISS', '--lightweight=']
            static_deps_flags = []

        pkg_config_path = os.getenv('PKG_CONFIG_PATH')
        if sys.platform == 'darwin':
            brew_deps = ['eigen@3', 'libyaml', 'fftw', 'ffmpeg', 'libsamplerate', 'libtag', 'chromaprint', 'libtensorflow']
            brew_pkg_paths = []
            for dep in brew_deps:
                try:
                    prefix = subprocess.check_output(['brew', '--prefix', dep], text=True).strip()
                except Exception:
                    continue
                brew_pkg_paths.extend([os.path.join(prefix, 'lib', 'pkgconfig'),
                                       os.path.join(prefix, 'share', 'pkgconfig')])
            if brew_pkg_paths:
                pkg_config_path = ':'.join(brew_pkg_paths + ([pkg_config_path] if pkg_config_path else []))
        pkg_config_flags = []
        if pkg_config_path:
            pkg_config_flags = [f'--pkg-config-path={pkg_config_path}']

        if var_skip_3rdparty in os.environ and os.environ[var_skip_3rdparty]=='1':
            print('Skipping building static 3rdparty dependencies (%s=1)' %  var_skip_3rdparty)
        elif sys.platform.startswith('win'):
            # The helper script is Debian-specific shell code and cannot be executed directly on Windows.
            print('Skipping building static 3rdparty dependencies on Windows (packaging/build_3rdparty_static_debian.sh is Linux-only)')
        else:
            subprocess.run('./packaging/build_3rdparty_static_debian.sh', check=True)

        if var_only_python in os.environ and os.environ[var_only_python]=='1':
            print('Skipping building the core libessentia library (%s=1)' %  var_only_python)
            subprocess.run([PYTHON,  'waf', 'configure', '--only-python',
                      '--prefix=tmp'] + static_deps_flags + windows_flags + macos_arm64_flags + pkg_config_flags, check=True)
        else:
            subprocess.run([PYTHON, 'waf', 'configure',
                      '--with-python', '--prefix=tmp'] + build_static_flags + static_deps_flags + windows_flags + macos_arm64_flags + pkg_config_flags, check=True)
        subprocess.run([PYTHON, 'waf'], check=True)
        subprocess.run([PYTHON, 'waf', 'install'], check=True)

        # Locate installed python package path across platforms.
        candidate_patterns = [
            # POSIX installs via waf/python distutils.
            os.path.join('tmp', 'lib', 'python*', '*-packages', 'essentia'),
            # Windows installs.
            os.path.join('tmp', 'Lib', 'site-packages', 'essentia'),
            # Alternate lowercase variant (some environments).
            os.path.join('tmp', 'lib', 'site-packages', 'essentia'),
        ]
        # Some waf + setuptools build environments (notably Windows cibuildwheel)
        # install the package directly into the active interpreter site-packages
        # while still honoring --prefix for non-Python artifacts.
        candidate_paths = []
        for key in ('purelib', 'platlib'):
            path = sysconfig.get_paths().get(key)
            if path:
                candidate_paths.append(os.path.join(path, 'essentia'))
        try:
            candidate_paths.extend([
                os.path.join(path, 'essentia') for path in site.getsitepackages()
            ])
        except Exception:
            pass
        user_site = site.getusersitepackages()
        if user_site:
            candidate_paths.append(os.path.join(user_site, 'essentia'))

        matches = []
        for pattern in candidate_patterns:
            matches.extend(glob.glob(pattern))
        for path in candidate_paths:
            if os.path.isdir(path):
                matches.append(path)

        if not matches:
            raise RuntimeError(
                "Could not locate installed essentia package directory under tmp/. "
                f"Tried: {candidate_patterns + candidate_paths}"
            )

        library = matches[0]


def get_git_version():
    """ try grab the current version number from git"""
    version = None
    if os.path.exists(".git"):
        try:
            version = os.popen("git describe --always --tags").read().strip()
        except Exception as e:
            print(e)
    return version


def get_version():
    version = open('VERSION', 'r').read().strip('\n')
    if version.count('-dev'):
        # Development version. Get the number of commits after the last release
        git_version = get_git_version()
        print('git describe:', git_version)
        # `git describe --always --tags` can return either:
        #   - "<tag>-<n>-g<sha>" when reachable from a tag
        #   - "<sha>" when no tag is reachable
        # In the latter case there is no commit-count token to parse.
        dev_commits = '0'
        if git_version:
            git_tokens = git_version.split('-')
            if len(git_tokens) >= 3 and git_tokens[-2].isdigit():
                dev_commits = git_tokens[-2]
        if not dev_commits.isdigit():
            print('Error parsing the number of dev commits: %s', dev_commits)
            dev_commits = '0'
        version += dev_commits
    return version


classifiers = [
    'Development Status :: 4 - Beta',
    'Intended Audience :: Developers',
    'Intended Audience :: Science/Research',
    'Topic :: Software Development :: Libraries',
    'Topic :: Multimedia :: Sound/Audio :: Analysis',
    'Topic :: Multimedia :: Sound/Audio :: Sound Synthesis',
    'Operating System :: POSIX',
    'Operating System :: MacOS :: MacOS X',
    #'Operating System :: Microsoft :: Windows',
    'Programming Language :: C++',
    'Programming Language :: Python',
    'Programming Language :: Python :: 3',
    'Programming Language :: Python :: 3.9',
    'Programming Language :: Python :: 3.10',
    'Programming Language :: Python :: 3.11',
    'Programming Language :: Python :: 3.12',
    'Programming Language :: Python :: 3.13',
]

description = 'Library for audio and music analysis, description and synthesis'
long_description = '''
Essentia is an open-source C++ library with Python bindings for audio analysis and audio-based music information retrieval. It contains an extensive collection of algorithms, including audio input/output functionality, standard digital signal processing blocks, statistical characterization of data, a large variety of spectral, temporal, tonal, and high-level music descriptors, and tools for inference with deep learning models. Designed with a focus on optimization in terms of robustness, computational speed, low memory usage, as well as flexibility, it is efficient for many industrial applications and allows fast prototyping and setting up research experiments very rapidly.

Website: https://essentia.upf.edu
'''

# Require tensorflow for the package essentia-tensorflow
# We are using version 2.5.0 as it is the newest version supported by the C API
# https://www.tensorflow.org/guide/versions
if project_name == 'essentia-tensorflow':
    description += ', with TensorFlow support'

module = Extension('name', sources=[])

setup(
    version=get_version(),
    description=description,
    long_description=long_description,
    author='Dmitry Bogdanov',
    author_email='dmitry.bogdanov@upf.edu',
    url='http://essentia.upf.edu',
    project_urls={
        "Documentation": "http://essentia.upf.edu",
        "Source Code": "https://github.com/MTG/essentia"
    },
    keywords='audio music sound dsp MIR',
    license='AGPLv3',
    platforms='any',
    classifiers=classifiers,
    ext_modules=[module],
    cmdclass={
        'build_ext': EssentiaBuildExtension,
        'install_lib': EssentiaInstall
    }
)
