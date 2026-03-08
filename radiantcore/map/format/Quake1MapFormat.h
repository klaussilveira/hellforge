#pragma once

#include "imapformat.h"
#include "Quake3MapReader.h"

namespace map
{

class Quake1MapReader : public Quake3MapReader
{
public:
	using Quake3MapReader::Quake3MapReader;

protected:
	void initPrimitiveParsers() override;
};

class Valve220MapReader : public Quake3MapReader
{
public:
	using Quake3MapReader::Quake3MapReader;

protected:
	void initPrimitiveParsers() override;
};

class Quake2MapReader : public Quake3MapReader
{
public:
	using Quake3MapReader::Quake3MapReader;

protected:
	void initPrimitiveParsers() override;
};

class Quake1MapFormat :
	public MapFormat,
	public std::enable_shared_from_this<Quake1MapFormat>
{
public:
	std::string getName() const override;
	StringSet getDependencies() const override;
	void initialiseModule(const IApplicationContext& ctx) override;
	void shutdownModule() override;

	const std::string& getMapFormatName() const override;
	const std::string& getGameType() const override;
	IMapReaderPtr getMapReader(IMapImportFilter& filter) const override;
	IMapWriterPtr getMapWriter() const override;
	bool allowInfoFileCreation() const override;
	bool canLoad(std::istream& stream) const override;
};

class Valve220MapFormat :
	public MapFormat,
	public std::enable_shared_from_this<Valve220MapFormat>
{
public:
	std::string getName() const override;
	StringSet getDependencies() const override;
	void initialiseModule(const IApplicationContext& ctx) override;
	void shutdownModule() override;

	const std::string& getMapFormatName() const override;
	const std::string& getGameType() const override;
	IMapReaderPtr getMapReader(IMapImportFilter& filter) const override;
	IMapWriterPtr getMapWriter() const override;
	bool allowInfoFileCreation() const override;
	bool canLoad(std::istream& stream) const override;
};

class Quake2MapFormat :
	public MapFormat,
	public std::enable_shared_from_this<Quake2MapFormat>
{
public:
	std::string getName() const override;
	StringSet getDependencies() const override;
	void initialiseModule(const IApplicationContext& ctx) override;
	void shutdownModule() override;

	const std::string& getMapFormatName() const override;
	const std::string& getGameType() const override;
	IMapReaderPtr getMapReader(IMapImportFilter& filter) const override;
	IMapWriterPtr getMapWriter() const override;
	bool allowInfoFileCreation() const override;
	bool canLoad(std::istream& stream) const override;
};

} // namespace map
