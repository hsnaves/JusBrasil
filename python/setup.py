from distutils.core import setup
from Cython.Build import cythonize

setup(
	name = 'PLSA module',
	ext_modules = cythonize("plsa.pyx"),
)
