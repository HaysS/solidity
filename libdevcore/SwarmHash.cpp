/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file SwarmHash.cpp
 */

#include <libdevcore/SwarmHash.h>

#include <libdevcore/Keccak256.h>
#include <libdevcore/picosha2.h>

using namespace std;
using namespace dev;

namespace
{

bytes toLittleEndian(size_t _size)
{
	bytes encoded(8);
	for (size_t i = 0; i < 8; ++i)
		encoded[i] = (_size >> (8 * i)) & 0xff;
	return encoded;
}

h256 swarmHashSimple(bytesConstRef _data, size_t _size)
{
	return keccak256(toLittleEndian(_size) + _data.toBytes());
}

h256 swarmHashIntermediate(string const& _input, size_t _offset, size_t _length)
{
	bytesConstRef ref;
	bytes innerNodes;
	if (_length <= 0x1000)
		ref = bytesConstRef(_input).cropped(_offset, _length);
	else
	{
		size_t maxRepresentedSize = 0x1000;
		while (maxRepresentedSize * (0x1000 / 32) < _length)
			maxRepresentedSize *= (0x1000 / 32);
		for (size_t i = 0; i < _length; i += maxRepresentedSize)
		{
			size_t size = std::min(maxRepresentedSize, _length - i);
			innerNodes += swarmHashIntermediate(_input, _offset + i, size).asBytes();
		}
		ref = bytesConstRef(&innerNodes);
	}
	return swarmHashSimple(ref, _length);
}

}

h256 dev::swarmHash(string const& _input)
{
	return swarmHashIntermediate(_input, 0, _input.size());
}

namespace
{
bytes varintEncoding(size_t _n)
{
	uint8_t leastSignificant = _n & 0x7f;
	size_t moreSignificant = _n >> 7;
	if (moreSignificant)
		return bytes{uint8_t(uint8_t(0x80) | leastSignificant)} + varintEncoding(moreSignificant);
	else
		return bytes{leastSignificant};
}

string base58Encode(bytes const& _data)
{
	static string const alphabet{"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"};
	bigint data(toHex(_data, HexPrefix::Add));
	string output;
	while (data)
	{
		output += alphabet[size_t(data % alphabet.size())];
		data /= alphabet.size();
	}
	reverse(output.begin(), output.end());
	return output;
}
}

bytes dev::ipfsHash(string const& _data)
{
	bytes lengthAsVarint = varintEncoding(_data.size());

	bytes protobufEncodedData;
	// Type: File
	protobufEncodedData += bytes{0x08, 0x02};
	if (!_data.empty())
	{
		// Data (length delimited bytes)
		protobufEncodedData += bytes{0x12};
		protobufEncodedData += lengthAsVarint;
		protobufEncodedData += asBytes(_data);
	}
	// filesize: length as varint
	protobufEncodedData += bytes{0x18} + lengthAsVarint;

	// TODO check what this actually refers to
	// TODO Handle "large" files with multiple blocks
	bytes blockData = bytes{0x0a} + varintEncoding(protobufEncodedData.size()) + protobufEncodedData;

	cout << toHex(protobufEncodedData) << endl;
	cout << toHex(blockData) << endl;
	// Multihash: sha2-256, 256 bits
	// TODO Do not go to hex and back
	bytes hash = bytes{0x12, 0x20} + fromHex(picosha2::hash256_hex_string(blockData));
	cout << "ipfs://" << base58Encode(hash) << endl;
	return hash;
}
