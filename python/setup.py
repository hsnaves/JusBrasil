from distutils.core import setup
from Cython.Build import cythonize
import numpy as np

setup(
	name = 'PLSA module',
	ext_modules = cythonize("plsa.pyx"),
	include_dirs = [np.get_include()],
)
