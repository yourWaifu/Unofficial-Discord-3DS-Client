#include <string>
#include <cstdio>
#include <vector>

class tokenFile {
public:
	tokenFile(const char* path) {
		//open file for reading
#ifdef _WIN32
		fopen_s(&fileHandle, path, "rb");
#else
		fileHandle = fopen(path, "rb");
#endif
		if (fileHandle == 0) {
				size = -1;
			return;
		}
		//get file length
		int position = ftell(fileHandle);
		fseek(fileHandle, position, SEEK_END);
		size = ftell(fileHandle);
		fseek(fileHandle, position, SEEK_SET);
	}

	const std::string getToken() {
		std::string buffer(size + 1, 0);
		fread(&buffer[0], 1, size, fileHandle);
		return &buffer[0];
	}

	const int getSize() {
		return size;
	}

	void close() {
		fclose(fileHandle);
	}
private:
	FILE* fileHandle;
	int size;
};