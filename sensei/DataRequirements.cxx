#include "DataRequirements.h"
#include "DataAdaptor.h"
#include "VTKUtils.h"
#include "Error.h"

#include <vtkDataObject.h>
#include <sstream>

namespace sensei
{

static
unsigned int getArrayNames(pugi::xml_node node, std::vector<std::string> &arrays)
{
  if (!node || !node.text())
    return -1;

  std::string text = node.text().as_string();

  // replace ',' with ' '
  size_t n = text.size();
  for (size_t i = 0; i < n; ++i)
    {
    if (text[i] == ',')
      text[i] = ' ';
    }

  std::istringstream iss(text);

  while (iss.good())
    {
    std::string array;
    iss >> array >> std::ws;
    arrays.push_back(array);
    }

  return arrays.size();
}

// --------------------------------------------------------------------------
DataRequirements::DataRequirements()
{
}

// --------------------------------------------------------------------------
DataRequirements::~DataRequirements()
{
}

// --------------------------------------------------------------------------
void DataRequirements::Clear()
{
  this->MeshNames.clear();
  this->MeshArrayMap.clear();
}

// --------------------------------------------------------------------------
int DataRequirements::Initialize(pugi::xml_node parent)
{
  this->Clear();

  int retVal = 0;
  int meshId = 0;

  // walk the children look for elements named mesh
  for (pugi::xml_node node = parent.child("mesh");
    node; node = node.next_sibling("mesh"))
    {
    // get the mesh name,, it is required
    if (!node.attribute("name"))
      {
      SENSEI_ERROR("Mesh " << meshId
        << " element is missing required attribute name")
      retVal = -1;
      continue;
      }

    std::string meshName = node.attribute("name").as_string();
    bool structureOnly = node.attribute("structure_only").as_int(0);

    this->MeshNames.insert(std::make_pair(meshName, structureOnly));

    // get cell data arrays, optional
    std::vector<std::string> arrays;
    if (getArrayNames(node.child("cell_arrays"), arrays))
      this->MeshArrayMap[meshName][vtkDataObject::CELL] = arrays;

    // get point data arrays, optional
    arrays.clear();
    if (getArrayNames(node.child("point_arrays"), arrays))
      this->MeshArrayMap[meshName][vtkDataObject::POINT] = arrays;

    meshId += 1;
    }

  return retVal;
}

//----------------------------------------------------------------------------
int DataRequirements::Initialize(DataAdaptor *adaptor)
{
  this->Clear();

  std::vector<std::string> meshNames;
  if (adaptor->GetMeshNames(meshNames))
    {
    SENSEI_ERROR("Failed to get mesh names")
    return -1;
    }

  unsigned int nMeshes = meshNames.size();
  for (unsigned int i = 0; i < nMeshes; ++i)
    {
    const std::string &meshName = meshNames[i];

    this->AddRequirement(meshName, false);

    int associations[] = {vtkDataObject::POINT, vtkDataObject::CELL};
    for (int j = 0; j < 2; ++j)
      {
      int association = associations[j];

      std::vector<std::string> arrays;
      if (adaptor->GetArrayNames(meshName, association, arrays))
        {
        SENSEI_ERROR("Failed to get "
          << VTKUtils::GetAttributesName(association)
          << " adaptor arrays on mesh \"" << meshName << "\"")
        return -1;
        }

      if (arrays.size())
        this->AddRequirement(meshName, association, arrays);
      }
    }

  return 0;
}

// --------------------------------------------------------------------------
int DataRequirements::AddRequirement(const std::string &meshName,
  bool structureOnly)
{
  this->MeshNames.insert(std::make_pair(meshName, structureOnly));
  return 0;
}

// --------------------------------------------------------------------------
int DataRequirements::AddRequirement(const std::string &meshName,
  int association, const std::vector<std::string> &arrays)
{
  if (meshName.empty())
    {
    SENSEI_ERROR("A mesh name is required")
    return -1;
    }

  // always add the mesh, mesh geometry can be used without
  // any arrays
  this->MeshNames.insert(std::make_pair(meshName, false));

  // only add arrays if there are any
  if (!arrays.empty())
    this->MeshArrayMap[meshName][association] = arrays;

  return 0;
}

// --------------------------------------------------------------------------
int DataRequirements::GetRequiredMesh(unsigned int id, std::string &mesh) const
{
  mesh = "";

  unsigned int nMeshes = this->MeshNames.size();
  if (id >= nMeshes)
    {
    SENSEI_ERROR("Index " << id << " is out of bounds, only "
      << nMeshes << " meshes")
    return -1;
    }

  MeshNamesType::const_iterator it = this->MeshNames.begin();

  for (unsigned int i = 0; i < id; ++i)
    ++it;

  mesh = it->first;

  return 0;
}

// --------------------------------------------------------------------------
int DataRequirements::GetRequiredMeshes(std::vector<std::string> &meshes) const
{
  meshes.clear();

  MeshNamesType::const_iterator it = this->MeshNames.begin();
  MeshNamesType::const_iterator end = this->MeshNames.end();

  for (; it != end; ++it)
    meshes.push_back(it->first);

  return meshes.size();
}

// --------------------------------------------------------------------------
unsigned int DataRequirements::GetNumberOfRequiredMeshes() const
{
  return this->MeshNames.size();
}

// --------------------------------------------------------------------------
int DataRequirements::GetRequiredArrays(const std::string &meshName,
  int association, std::vector<std::string> &arrays) const
{
  arrays.clear();
  MeshArrayMapType::const_iterator it = this->MeshArrayMap.find(meshName);
  if (it != this->MeshArrayMap.end())
    {
    AssocArrayMapType::const_iterator ait = it->second.find(association);
    if (ait != it->second.end())
      arrays = ait->second;
    }
  return 0;
}

// --------------------------------------------------------------------------
int DataRequirements::GetNumberOfRequiredArrays(const std::string &meshName,
  int association, unsigned int &nArrays) const
{
  nArrays = 0;
  MeshArrayMapType::const_iterator it = this->MeshArrayMap.find(meshName);
  if (it != this->MeshArrayMap.end())
    {
    AssocArrayMapType::const_iterator ait = it->second.find(association);
    if (ait != it->second.end())
      nArrays = ait->second.size();
    }
  return 0;
}

// --------------------------------------------------------------------------
MeshRequirementsIterator DataRequirements::GetMeshRequirementsIterator() const
{
  MeshRequirementsIterator it(this->MeshNames);
  return it;
}

// --------------------------------------------------------------------------
ArrayRequirementsIterator DataRequirements::GetArrayRequirementsIterator(
  const std::string &meshName) const
{
  MeshArrayMapType::const_iterator it = this->MeshArrayMap.find(meshName);
  if (it != this->MeshArrayMap.end())
    {
    ArrayRequirementsIterator ait(it->second);
    return ait;
    }
  SENSEI_ERROR("No mesh named \"" << meshName << "\"")
  return ArrayRequirementsIterator();
}

}
