#include <cstdio>
#include <cstring>
#include <assert.h>
#include <string>
#include <sstream>
#include <vector>
#include <tinyxml.h>

void
dump_text(const TiXmlNode *node, std::ostream &str);

class function_param
{
public:
  void
  set(const TiXmlElement *node);

  std::string m_type;
  std::string m_name;
  std::string m_name_suffix;
};

class Command
{
public:
  explicit
  Command(const TiXmlElement *node);
  
  function_param m_proto;
  std::vector<function_param> m_params;
};

///////////////////////////
// function_param methods
void
function_param::
set(const TiXmlElement *node)
{
  bool name_found(false);

  for (const TiXmlNode* p = node->FirstChild(); p; p = p->NextSibling()) 
    {
      const TiXmlElement *ele;

      ele = p->ToElement();

      if (ele && std::strcmp(ele->Value(), "name") == 0)
        {
          const TiXmlText *txt;

          assert(ele->FirstChild());
          txt = ele->FirstChild()->ToText();

          assert(txt);
          m_name = txt->Value();
          name_found = true;
        }
      else
        {
          std::ostringstream str;
          dump_text(p, str);
          if (name_found)
            {
              m_name_suffix += str.str();
            }
          else
            {
              m_type += str.str();
            }
        }
    }
}

///////////////////////////////
// Command methods
Command::
Command(const TiXmlElement *node)
{
  assert(std::strcmp(node->Value(), "command") == 0);
  for (const TiXmlNode* p = node->FirstChild(); p; p = p->NextSibling())
    {
      const TiXmlElement *ele;

      ele = p->ToElement();
      if (!ele)
        continue;

      if (std::strcmp(ele->Value(), "proto") == 0)
        {
          m_proto.set(ele);
        }
      else if (std::strcmp(ele->Value(), "param") == 0)
        {
          function_param Q;
          Q.set(ele);
          if (!Q.m_name.empty() && !Q.m_type.empty())
            {
              m_params.push_back(Q);
            }
        }
    }
}

///////////////////////////////
// global methods
void
dump_text(const TiXmlNode *node, std::ostream &str)
{
  const TiXmlText* pText;

  pText = node->ToText();
  if (pText)
    {
      str << pText->Value() << " ";
    }
  for (const TiXmlNode* p = node->FirstChild(); p; p = p->NextSibling()) 
    {
      dump_text(p, str);
    }
}

void
process_command(const TiXmlElement *command)
{
  Command Q(command);

  if (Q.m_proto.m_type.empty()
      || Q.m_proto.m_name.empty())
    {
      return;
    }

  if (Q.m_proto.m_type == "void" || Q.m_proto.m_type == "void ")
    {
      std::printf("FUNCTION_ENTRY(%s, (",
                  Q.m_proto.m_name.c_str());
    }
  else
    {
      std::printf("FUNCTION_ENTRY_RET(%s, %s, (",
                  Q.m_proto.m_type.c_str(),
                  Q.m_proto.m_name.c_str());
    }

  for(auto iter = Q.m_params.begin(); iter != Q.m_params.end(); ++iter)
    {
      if (iter != Q.m_params.begin())
        {
          std::printf(", ");
        }
      std::printf("%s %s%s", iter->m_type.c_str(), iter->m_name.c_str(),
                  iter->m_name_suffix.c_str());
    }
  std::printf("), (");
  
  for(auto iter = Q.m_params.begin(); iter != Q.m_params.end(); ++iter)
    {
      if (iter != Q.m_params.begin())
        {
          std::printf(", ");
        }
      std::printf("%s", iter->m_name.c_str());
    }
  std::printf("))\n");
}

void
process_node(const TiXmlNode *node)
{
  if (!node)
    return;

  const TiXmlElement *ele;
  ele = node->ToElement();
  
  if (ele && std::strcmp(ele->Value(), "command") == 0)
    {
      process_command(ele);
    }

  for (const TiXmlNode* p = node->FirstChild(); p; p = p->NextSibling()) 
    {
      process_node(p);
    }
}

void
process_xml(const TiXmlNode *node, unsigned int indent = 0)
{
  if (!node)
    return;

  for(unsigned int i = 0; i < indent; ++i)
    {
      std::printf(" ");
    }

  int t = node->Type();
  switch (t)
    {
    case TiXmlNode::TINYXML_DOCUMENT:
      std::printf("Document");
      break;

    case TiXmlNode::TINYXML_ELEMENT:
      std::printf("Element [%s]", node->Value());
      break;

    case TiXmlNode::TINYXML_COMMENT:
      std::printf("Comment: [%s]", node->Value());
      break;

    case TiXmlNode::TINYXML_UNKNOWN:
      std::printf("Unknown");
      break;

    case TiXmlNode::TINYXML_TEXT:
      const TiXmlText* pText;
      pText = node->ToText();
      std::printf("Text: [%s]", pText->Value());
      break;

    case TiXmlNode::TINYXML_DECLARATION:
      std::printf("Declaration");
      break;
    default:
      break;
    }
  std::printf("\n");

  for (const TiXmlNode* p = node->FirstChild(); p; p = p->NextSibling()) 
    {
      process_xml(p, indent + 1);
    }
}

enum process_mode_t
  {
    dump_xml,
    generate_gl_functions,
  };

void
process_file(const char *filename, enum process_mode_t mode)
{
  TiXmlDocument doc(filename);
  if (doc.LoadFile())
    {
      switch (mode)
        {
        case dump_xml:
          process_xml(&doc);
          break;

        case generate_gl_functions:
          process_node(&doc);
          break;
        }
    }
  else
    {
      std::fprintf(stderr, "Failed to open file %s\n", filename);
    }
}

int
main(int argc, char **argv)
{
  enum process_mode_t mode = generate_gl_functions;
  int start = 1;
  
  if (argc > 1 && std::strcmp(argv[1], "-x") == 0)
    {
      mode = dump_xml;
      ++start;
    }
  
  for(int i = start; i < argc; ++i)
    {
      process_file(argv[i], mode);
    }
  return 0;
}
