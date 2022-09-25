'''
# ckdl - KDL reading and writing using a C backend

**ckdl** is a C library that implements reading and writing a the
[KDL Document Language](https://kdl.dev/).

This package lets Python programs read and write KDL files, using
*ckdl* as a back-end.

Install with

    pip install ckdl

## Examples

### Reading

```pycon
>>> import ckdl
>>> kdl_txt = """
... best-primes 2 3 5 7
... colours importance=(%)1000 { green; blue shade="синий"; blue shade="голубой"; violet }
... """
>>> doc = ckdl.parse(kdl_txt)
>>> doc
<Document; 2 nodes>
>>> doc[0]
<Node best-primes; 4 args>
>>> doc[0].args
[2, 3, 5, 7]
>>> doc[1].properties
{'importance': <Value (%)1000>}
>>> doc[1].properties['importance'].value
1000
>>> doc[1].properties['importance'].type_annotation
'%'
>>> doc[1].children
[<Node green>, <Node blue; 1 property>, <Node blue; 1 property>, <Node violet>]
>>> doc[1].children[1].properties['shade']
'синий'
```

### Writing

```pycon
>>> import ckdl
>>> mydoc = ckdl.Document(ckdl.Node("best-primes", 7, 11, 13), ckdl.Node("worst-primes", ckdl.Value("undoubtedly", 5)))
>>> print(str(mydoc))
best-primes 7 11 13
worst-primes (undoubtedly)5
```
'''

from skbuild import setup

setup(
    name="ckdl",
    version="0.1",
    description="KDL parser and writer with a C back-end",
    long_description=__doc__,
    long_description_content_type="text/markdown",
    author="Thomas Jollans",
    url="https://github.com/tjol/ckdl",
    project_urls={
        "Documentation": "https://ckdl.readthedocs.io/"
    },
    keywords="kdl parser configuration",
    classifiers = [
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3"
    ],
    package_dir={"": "bindings/python/src"},
    packages=["ckdl"],
    cmake_args=[
        "-DBUILD_TESTS:BOOL=OFF",
        "-DBUILD_KDLPP:BOOL=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON",
    ],
    cmake_install_target="install-ckdl-py",
)
