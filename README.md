# What is this repository ?

This repository contains one single header file, which contains functions to load and save _NumPy_
arrays saved in **NPY** format, along with a license and this very file.

# How do I install this library ?

You don't. Just download the _npy.hpp_ header file into your project, and `#include` it wherever you
judge necessary.

# How do I use this library ?

Once you've included the header, you can read a _NumPy_ array with `npy::Array`'s static method
`load` the following way :

```cpp
auto array = npy::Array::load("input_file.npy")
```

The `npy::Array` class contains then two vectors :
- `shape` : the number of elements for each dimension of the array
- `data` : the data contained in the array in linear form

You can then write an `npy::Array` to a _NumPy_ array using the `save` method :

```cpp
	array.save("output_file.npy")
```

## Is there any equivalent to `np.zeros` ?

No, this library is solely intended to provide load/save routines, nothing else.
If you really need to create an _M x N x O_ `npy::Array` filled with zeroes, I suggest the following
call to its _constructor_ :

```cpp
npy::Array{ { M, N, O }, std::vector<double>(M * N * O, 0.0) }
```

Adapt the dimensions to your need.

## How do I convert this to a format usable by library XYZ ?

As the array data is contained into an `std::vector`, one can obtain a _raw pointer_ to the data by
calling its `data` method.
So if your `npy::Array` is called _array_, you do the following call :

```cpp
array.data.data()
```

From there on, _library XYZ_ hopefully provides some procedure to convert the pointed data into a
suitable form.

## Why use `std::vector` to store the data ?

Because its destructor will handle the deallocation of the data.
In turn, please avoid dangling pointers.

## Do I need specific flags to compile ?

I have tested compiling with C++17 and C++20.

# Limitations

## NPY versions

Loading is implemented for NPY version _1.0_ and _2.0_.
Support for version _3.0_ is not intended.

Furthermore, saving is only implemented for version _1.0_.

## Array shape

Loading is implemented for any amount of dimensions.
I chose to implement saving with a _format string_ of 246 bytes, which means that the dimensions of
the array should be representable with maximum 190 characters as comma-separated numbers followed by
one space.

## Array `dtype`

Only the equivalents of `np.float32` (a C `float`) and `np.float64` (a C `double`) are supported.

## Array contents
I haven't tested `NaN`, `inf` and `-inf`.
