//	output.hpp
//		write binary output files

#ifndef OUTPUT_HPP
#define OUTPUT_HPP

class OutputFile {
public:
	OutputFile(const char* fileName);
	~OutputFile();

	void	SeekTo(long offset);
	void	WriteByte(ubyte);
	void	WriteOp(ubyte op) { WriteByte(op); }
	void	WriteWord(SCIWord);
	void	Write(const void*, size_t);
	int	Write(const char*);

protected:
	int			fd;
	const char*	fileName;
};

void OpenObjFiles(OutputFile** heapOut, OutputFile** hunkOut);

extern Bool	highByteFirst;

#endif
