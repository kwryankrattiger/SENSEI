#include "XMLUtils.h"
#include "Error.h"

#include <pugixml.hpp>

#include <cstdio>
#include <cstring>
#include <errno.h>


namespace sensei
{

namespace XMLUtils
{

//----------------------------------------------------------------------------
int RequireAttribute(const pugi::xml_node &node, const char *attributeName)
{
  if (!node.attribute(attributeName))
    {
    SENSEI_ERROR(<< node.name() << " is missing required attribute " << attributeName)
    return -1;
    }
  return 0;
}

//----------------------------------------------------------------------------
int RequireChild(const pugi::xml_node &node, const char *childName)
{
  if (!node.child(childName))
    {
    SENSEI_ERROR(<< node.name() << " is missing required child element " << childName)
    return -1;
    }
  return 0;
}

//----------------------------------------------------------------------------
int Parse(MPI_Comm comm, const std::string &filename, pugi::xml_document &doc)
{
  int rank = 0;
  MPI_Comm_rank(comm, &rank);

  unsigned long nbytes = 0;
  char *buffer = nullptr;
  if (rank == 0)
    {
    FILE *f = fopen(filename.c_str(), "rb");
    if (f)
      {
      setvbuf(f, nullptr, _IONBF, 0);
      fseek(f, 0, SEEK_END);
      nbytes = ftell(f);
      fseek(f, 0, SEEK_SET);
      buffer = static_cast<char*>(pugi::get_memory_allocation_function()(nbytes));
      unsigned long nread = fread(buffer, 1, nbytes, f);
      fclose(f);
      if (nread == nbytes)
        {
        MPI_Bcast(&nbytes, 1, MPI_UNSIGNED_LONG, 0, comm);
        MPI_Bcast(buffer, nbytes, MPI_CHAR, 0, comm);
        }
      else
        {
        SENSEI_ERROR("read error on \""  << filename << "\"" << endl << strerror(errno))
        nbytes = 0;
        MPI_Bcast(&nbytes, 1, MPI_UNSIGNED_LONG, 0, comm);
        return -1;
        }
      }
    else
      {
      SENSEI_ERROR("failed to open \""  << filename << "\"" << endl << strerror(errno))
      MPI_Bcast(&nbytes, 1, MPI_UNSIGNED_LONG, 0, comm);
      return -1;
      }
    }
  else
    {
    MPI_Bcast(&nbytes, 1, MPI_UNSIGNED_LONG, 0, comm);
    if (!nbytes)
      return -1;
    buffer = static_cast<char*>(pugi::get_memory_allocation_function()(nbytes));
    MPI_Bcast(buffer, nbytes, MPI_CHAR, 0, comm);
    }

  pugi::xml_parse_result result = doc.load_buffer_inplace_own(buffer, nbytes);
  if (!result)
    {
    SENSEI_ERROR("XML [" << filename << "] parsed with errors, attr value: ["
      << doc.child("node").attribute("attr").value() << "]" << endl
      << "Error description: " << result.description() << endl
      << "Error offset: " << result.offset << endl)
    return -1;
    }

  return 0;
}

//----------------------------------------------------------------------------
int ParseNameValuePairs(const pugi::xml_node &node,
  std::vector<std::string> &names, std::vector<std::string> &values)
{
  std::string strData = node.text().as_string();

  std::size_t curr = strData.find_first_not_of(" ,\t\n", 0);
  std::size_t next = std::string::npos;

  while (curr != std::string::npos)
    {
    // find next delimiter, not including white space
    next = strData.find_first_of(",\n\t", curr + 1);
    std::string tmp = strData.substr(curr, next - curr);

    // skip a string full of white space
    std::size_t n0 = tmp.find_first_not_of(" ");
    if (n0 == std::string::npos)
      continue;

    char name[128];
    char value[128];

    name[127] = '\0';
    value[127] = '\0';

    if (sscanf(tmp.c_str(), "%127s = %127s", name, value) == 2)
      {
      names.push_back(name);
      values.push_back(value);
      }
    else
      {
      SENSEI_WARNING("Failed to parse \"name = value\" in \""
        << tmp << "\"")
      }

    curr = strData.find_first_not_of(" ,\t\n", next);
    }

  return 0;
}

}
}
