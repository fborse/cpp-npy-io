#ifndef __NPY_LOADER_HPP__
#define __NPY_LOADER_HPP__

#include <vector>
#include <string>

#include <fstream>
#include <regex>
#include <stdexcept>

namespace npy
{
	struct Array
	{
		std::vector<size_t> shape;
		std::vector<double> data;

		static Array load(const std::string &filename);
		void save(const std::string &filename) const;
	};

	struct ArrayReader
	{
		explicit ArrayReader(const std::string &filename):
			stream(filename, std::ifstream::binary)
		{
			if (!stream.good())
				throw std::runtime_error(std::string("Failed to open ") + filename);
		}

		Array exec()
		{
			checkHeaderBase();
			readFormatString();
			parseFormatString();
			parseShapeString();

			return readArray();
		}

	private:
		std::ifstream stream;

	//	header base
		std::string magic = "\x93NUMPY";
		unsigned char major = 1, minor = 0;

	//	format string
		unsigned int header_len = 0;
		std::string format_string;
		std::string shape_string;

		enum {
			F4 = sizeof(float),
			F8 = sizeof(double)
		} float_sizes;
		std::vector<size_t> shape;

		void checkHeaderBase()
		{
			stream.read(magic.data(), magic.size() * sizeof(char));

			if (magic != "\x93NUMPY")
				throw std::runtime_error("not a NumPy array");

			stream.read((char *) &major, sizeof(unsigned char));
			stream.read((char *) &minor, sizeof(unsigned char));

			if (major == 0 || major > 2 || minor != 0)
				throw std::runtime_error("we only support NPY version 1.0 and 2.0");
		}

		void readFormatString()
		{
			if (major == 1)
				stream.read((char *) &header_len, sizeof(unsigned short));
			else
				stream.read((char *) &header_len, sizeof(unsigned int));

			format_string.resize(header_len);
			stream.read(format_string.data(), header_len * sizeof(char));
		}

	//	"{'descr': '<f8', 'fortran_order': False, 'shape': (4, 32, 48, 218), }                \n"
		void parseFormatString()
		{
			std::smatch match;
		//	ensure that dtype is little-endian float32 or float64
			std::regex_search(format_string, match, std::regex("'descr':\\s*'(.+?)'"));
			if (match[1] == "<f4")
				float_sizes = F4;
			else if (match[1] == "<f8")
				float_sizes = F8;
			else
				throw std::runtime_error("we only support dtype = float64 or float32");

		//	ensure that it's not using FORTRAN order
			std::regex_search(format_string, match, std::regex("'fortran_order':\\s*(\\w+)"));
			if (match[1] != "False")
				throw std::runtime_error("we don't support FORTRAN ordered arrays");

		//	get shape string
			std::regex_search(format_string, match, std::regex("'shape':\\s*\\((.+)\\)"));
			shape_string = match[1];
		}

		void parseShapeString()
		{
			std::smatch match;

			std::string str = shape_string;
			while (std::regex_search(str, match, std::regex("(\\d+)")))
			{
				shape.push_back(std::stoul(match[1]));
				str = match.suffix().str();
			}

			if (shape.size() == 0)
				throw std::runtime_error("0-dimensional arrays unsupported");
			for (auto &dim: shape)
				if (dim == 0)
					throw std::runtime_error("one dimension is zero");
		}

		Array readArray()
		{
			size_t n = 1;
			for (auto &dim: shape)
				n *= dim;

			Array array;

			array.shape = shape;
			array.data.resize(n);

			if (float_sizes == F8)
			{
				stream.read((char *) array.data.data(), n * sizeof(double));
			}
			else
			{
				std::vector<float> xs(n);
				stream.read((char *) xs.data(), n * sizeof(float));

				for (size_t i = 0; i < n; ++i)
					array.data[i] = xs[i];
			}

			return array;
		}
	};

	inline Array Array::load(const std::string &filename)
	{
		return ArrayReader(filename).exec();
	}

	inline void ensure_legit(const Array &array)
	{
		if (array.shape.empty())
			throw std::runtime_error("cannot save an empty array");

		size_t product = 1;
		for (auto &dim: array.shape)
			if (dim == 0)
				throw std::runtime_error("cannot save an array when one dimension is zero");
			else
				product *= dim;

		const size_t expected = array.data.size();
		if (product != expected)
			throw std::runtime_error("size mismatch between the array shape and the array data");
	}

	inline std::string join(const std::vector<size_t> &xs)
	{
		std::string out = "";

		auto it = std::cbegin(xs);
		out += std::to_string(*it);

		for (++it; it != std::cend(xs); ++it)
			out += ", " + std::to_string(*it);

		return out;
	}

	void Array::save(const std::string &filename) const
	{
		ensure_legit(*this);

		std::ofstream stream(filename, std::ofstream::binary);
		if (!stream.good())
			throw std::runtime_error(std::string("Failed to open ") + filename);

	//	header
		std::string magic = "\x93NUMPY";
		unsigned char major = 1, minor = 0;
		unsigned short header_len = 256 - 10;

	//	"{'descr': '<f8', 'fortran_order': False, 'shape': (4, 32, 48, 218), }                \n"
		std::string format_string = "{'descr': '<f8', 'fortran_order': False, 'shape': (";
		format_string += join(shape) + "), }";
		format_string.resize(header_len, ' ');
		format_string.back() = '\n';

	//	actual writing to the file
		stream.write(magic.data(), magic.size() * sizeof(char));
		stream.write((char *) &major, sizeof(unsigned char));
		stream.write((char *) &minor, sizeof(unsigned char));

		stream.write((char *) &header_len, sizeof(unsigned short));
		stream.write(format_string.data(), format_string.size() * sizeof(char));

		stream.write((char *) data.data(), data.size() * sizeof(double));
	}
}

#endif
