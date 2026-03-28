#include "AasFileInterface.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "iaasfile.h"
#include "imap.h"
#include "iarchive.h"
#include "ifilesystem.h"
#include "itextstream.h"

namespace script
{

std::vector<map::AasFileInfo> AasFileInterface::getAasFilesForCurrentMap()
{
	auto fileList = GlobalAasFileManager().getAasFilesForMap(GlobalMapModule().getMapName());
	return { fileList.begin(), fileList.end() };
}

map::IAasFilePtr AasFileInterface::loadAasFile(const std::string& absolutePath)
{
	ArchiveTextFilePtr file = GlobalFileSystem().openTextFileInAbsolutePath(absolutePath);

	if (!file)
	{
		rWarning() << "[AAS] Could not open file: " << absolutePath << std::endl;
		return {};
	}

	std::istream stream(&file->getInputStream());
	auto loader = GlobalAasFileManager().getLoaderForStream(stream);

	if (!loader || !loader->canLoad(stream))
	{
		rWarning() << "[AAS] No suitable loader for: " << absolutePath << std::endl;
		return {};
	}

	stream.seekg(0, std::ios_base::beg);
	return loader->loadFromStream(stream);
}

void AasFileInterface::registerInterface(py::module& scope, py::dict& globals)
{
	py::class_<map::AasType> aasType(scope, "AasType");
	aasType.def_readonly("entityDefName", &map::AasType::entityDefName);
	aasType.def_readonly("fileExtension", &map::AasType::fileExtension);
	aasType.def("__repr__", [](const map::AasType& t) {
		return "<AasType '" + t.fileExtension + "'>";
	});

	py::class_<map::AasFileInfo> aasFileInfo(scope, "AasFileInfo");
	aasFileInfo.def_readonly("absolutePath", &map::AasFileInfo::absolutePath);
	aasFileInfo.def_readonly("type", &map::AasFileInfo::type);
	aasFileInfo.def("__repr__", [](const map::AasFileInfo& i) {
		return "<AasFileInfo '" + i.absolutePath + "'>";
	});

	py::class_<map::IAasFile::Area> aasArea(scope, "AasArea");
	aasArea.def_readonly("numFaces", &map::IAasFile::Area::numFaces);
	aasArea.def_readonly("firstFace", &map::IAasFile::Area::firstFace);
	aasArea.def_readonly("bounds", &map::IAasFile::Area::bounds);
	aasArea.def_readonly("center", &map::IAasFile::Area::center);
	aasArea.def_readonly("flags", &map::IAasFile::Area::flags);
	aasArea.def_readonly("contents", &map::IAasFile::Area::contents);
	aasArea.def_readonly("cluster", &map::IAasFile::Area::cluster);
	aasArea.def_readonly("travelFlags", &map::IAasFile::Area::travelFlags);
	aasArea.def("__repr__", [](const map::IAasFile::Area& a) {
		return "<AasArea center=(" +
			std::to_string(a.center.x()) + ", " +
			std::to_string(a.center.y()) + ", " +
			std::to_string(a.center.z()) + ") flags=" +
			std::to_string(a.flags) + ">";
	});

	py::class_<map::IAasFile, map::IAasFilePtr> aasFile(scope, "AasFile");
	aasFile.def("getNumAreas", &map::IAasFile::getNumAreas);
	aasFile.def("getArea", &map::IAasFile::getArea, py::return_value_policy::reference);
	aasFile.def("getNumVertices", &map::IAasFile::getNumVertices);
	aasFile.def("getVertex", &map::IAasFile::getVertex, py::return_value_policy::reference);
	aasFile.def("getNumEdges", &map::IAasFile::getNumEdges);
	aasFile.def("getNumFaces", &map::IAasFile::getNumFaces);
	aasFile.def("getNumPlanes", &map::IAasFile::getNumPlanes);

	py::class_<AasFileInterface> aasManager(scope, "AasFileManager");
	aasManager.def("getAasFilesForCurrentMap", &AasFileInterface::getAasFilesForCurrentMap);
	aasManager.def("loadAasFile", &AasFileInterface::loadAasFile);

	globals["GlobalAasFileManager"] = this;
}

} // namespace script
