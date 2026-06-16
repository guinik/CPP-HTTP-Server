#include "HTTPRequest.hpp"
#include <stdexcept>

std::vector<std::string> splitByDelimiter(const std::string& string, const std::string& delimiter)
{

	size_t posStart = 0;
	size_t posEnd;
	std::vector<std::string> result;
	while ((posEnd = string.find(delimiter, posStart)) != std::string::npos) {
		std::string subString = string.substr(posStart, posEnd - posStart);
		result.push_back(subString);
		posStart = posEnd + delimiter.length();
	}

	result.push_back(string.substr(posStart));
	return result;
}


HTTPHead parseRawBytesHeadRequest(const std::string& rawRequest) {
	auto pos = rawRequest.find("\r\n");

	std::string firstLine = rawRequest.substr(0, pos);
	std::vector<std::string> firstLineVector = splitByDelimiter(firstLine, " ");

	if (firstLineVector.size() != 3) {
		throw std::runtime_error("First Line parsing is not consistent");
	}


	size_t lastPos = pos + 2;
	size_t newPos;
	CaseInsensitiveMap headerMap;

	while ((newPos = rawRequest.find("\r\n", lastPos)) != std::string::npos) {

		auto newLine = rawRequest.substr(lastPos, newPos - lastPos);
		if (newLine.empty()) {
			lastPos = lastPos + 2;
			break;
		}


		size_t colonPosition = newLine.find(":");
		if (colonPosition == std::string::npos) {
			throw std::runtime_error("Malformed header");
		}
		std::string key = newLine.substr(0, colonPosition);
		std::string value = newLine.substr(colonPosition + 1, newLine.length() - (colonPosition + 1));
		size_t start = value.find_first_not_of(" ");
		if (start != std::string::npos)
			value = value.substr(start);
		else
			value = "";


		headerMap[key] = value;
		lastPos = newPos + 2;
	}

	return HTTPHead{
		 .method = firstLineVector[0],
		 .path = firstLineVector[1],
		 .version = firstLineVector[2],
		 .headers = headerMap,
	};
};

HTTPBody parseRawBytesBodyRequest(const std::string& rawBody, const std::string& contentType ) {


	return HTTPBody{
			.raw = rawBody,
			.contentType = contentType
	};
};

HTTPRequest constructRequest(const HTTPHead& head, const HTTPBody& body) 
{
	return HTTPRequest{
		.head = head,
		.body = body
	};
};