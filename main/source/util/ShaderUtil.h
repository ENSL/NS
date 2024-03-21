#pragma once

#include <string>

// Utitlity class to load, compile and attach a vertex- and fragment shader to a program
class ShaderUtil
{

private: 
	unsigned int mProgramId;

	//unsigned int GetCompiledShader(unsigned int shader_type, const std::string& shader_source, const std::string& path);
	unsigned int GetCompiledShader(unsigned int shader_type, const std::string& shader_source);

public:
	ShaderUtil() {}
	~ShaderUtil() {}

	// Load a vertex and a fragment shader from file
	//bool LoadFromFile(const std::string& vertexShaderFile, const std::string& fragmentShaderFile);
	bool LoadFromString(const std::string& vertexShader, const std::string& fragmentShader);

	// Use the program
	void Use();

	// Delete the program
	void Delete();

	// Give the programID
	unsigned int GetProgramID() { return mProgramId; }

};

