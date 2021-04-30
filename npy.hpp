#ifndef __NPY_LOADER_HPP__
#define __NPY_LOADER_HPP__

#include <vector>
#include <string>

#include <fstream>
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

		std::string getDataFormat()
		{
			const size_t descr = format_string.find("'descr'");
			if (descr == std::string::npos)
				throw std::runtime_error("no 'descr' field in format string");

			const size_t opening = 1 + format_string.find("'", descr + 7);
			const size_t closing = format_string.find("'", opening);

			const auto it = format_string.cbegin();
			return std::string(it + opening, it + closing);
		}

		bool isFortranOrder()
		{
			const size_t fortran = format_string.find("'fortran_order'");
			if (fortran == std::string::npos)
				throw std::runtime_error("no 'fortran_order' field in format string");

			const size_t true_pos = format_string.find("True", fortran);
			const bool got_true = (true_pos != std::string::npos);
			const size_t false_pos = format_string.find("False", fortran);
			const bool got_false = (false_pos != std::string::npos);

			return !(got_false && !got_true);
		}

		std::string getShapeString()
		{
			const size_t shape = format_string.find("'shape'");
			if (shape == std::string::npos)
				throw std::runtime_error("no 'shape' field in format string");

			const size_t opening = 1 + format_string.find("(", shape);
			const size_t closing = format_string.find(")", opening);

			const auto it = format_string.cbegin();
			return std::string(it + opening, it + closing);
		}

	//	"{'descr': '<f8', 'fortran_order': False, 'shape': (4, 32, 48, 218), }                \n"
		void parseFormatString()
		{
		//	ensure that dtype is little-endian float32 or float64
			std::string dtype = getDataFormat();
			if (dtype == "<f4")
				float_sizes = F4;
			else if (dtype == "<f8")
				float_sizes = F8;
			else
				throw std::runtime_error("we only support dtype = float32 or float64");

			if (isFortranOrder())
				throw std::runtime_error("we don't support FORTRAN ordered arrays");

			shape_string = getShapeString();
		}

		void parseShapeString()
		{
			size_t start = 0;
			size_t comma = shape_string.find(",");

			while (comma != std::string::npos)
			{
				const auto it = shape_string.cbegin() + start;
				const auto jt = shape_string.cbegin() + comma;
				shape.push_back(std::stoul(std::string(it, jt)));

				start = comma + 1;
				comma = shape_string.find(",", start);
			}

			const auto it = shape_string.cbegin() + start;
			const auto jt = shape_string.cend();
			shape.push_back(std::stoul(std::string(it, jt)));

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

	//	actual writing of the file
		stream.write(magic.data(), magic.size() * sizeof(char));
		stream.write((char *) &major, sizeof(unsigned char));
		stream.write((char *) &minor, sizeof(unsigned char));

		stream.write((char *) &header_len, sizeof(unsigned short));
		stream.write(format_string.data(), format_string.size() * sizeof(char));

		stream.write((char *) data.data(), data.size() * sizeof(double));
	}
}

#endif
