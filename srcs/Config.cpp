#include "package.hpp"

extern	std::vector<Server> g_servers;
extern	bool				g_state;

/**
 *		Constructor
 */
Config::Config(Dispatcher &dispatcher): _dispatcher(dispatcher)
{

}

Config::~Config()
{

}

void			Config::exit(int sig)
{
  (void)sig;

  std::cout << "\n" << "exiting...\n";
  g_state = false;
}

std::string		Config::readFile(char *file)
{
  int 				fd;
  int					ret;
  char				buf[4096];
  std::string			parsed;

  fd = open(file, O_RDONLY);
  while ((ret = read(fd, buf, 4095)) > 0)
  {
    buf[ret] = '\0';
    parsed += buf;
  }
  close(fd);
  return (parsed);
}

/**
 *		@brief	매개변수로 주어진 config파일을 파싱해서 global server	에 데이터를 저장한다.
 *
 *		@param1 file: config file(args[1])
 *		@param2 servers: global server
 */
void			Config::parse(char *file, std::vector<Server> &servers)
{
  size_t				  	d;
  size_t				  	nb_line;
  std::string				context;
  std::string				buffer;
  std::string				line;
  Server				  	server(_dispatcher);
  config				  	tmp;
  bool					    http_flag;

  // read file
  buffer = readFile(file);
  nb_line = 0;
  http_flag = false;
  if (buffer.empty())
    throw(Config::InvalidConfigFileException(nb_line));
  // read buffer
  while (!buffer.empty())
  {
    ft::getline(buffer, line);
    nb_line++;
    line = ft::trim(line);
    if (!line.compare(0, 4, "http") && line[line.size() - 1] == '{' && http_flag == false)
    {
      http_flag = true;
      continue;
    }
    if (http_flag == true && line[0] == '}')
      break;

    if (!line.compare(0, 6, "server"))
    {
      while (ft::isspace(line[6]))
        line.erase(6, 1);
      if (line[6] != '{')
        throw(Config::InvalidConfigFileException(nb_line));
      if (!line.compare(0, 7, "server{"))
      {
        d = 7;
        while (ft::isspace(line[d]))
          line.erase(7, 1);
        if (line[d])
          throw(Config::InvalidConfigFileException(nb_line));
        // get block data
        getContent(buffer, context, line, nb_line, tmp);
        // port check
        std::vector<Server>::iterator it(servers.begin());
        while (it != servers.end())
        {
          if (tmp["server|"]["listen"] == it->_conf.back()["server|"]["listen"])
          {
            std::vector<config>::iterator it2(it->_conf.begin());
            while (it2 != it->_conf.end())
            {
              if (tmp["server|"]["server_name"] == (*it2)["server|"]["server_name"])
                throw(Config::InvalidConfigFileException(nb_line));
              it2++;
            }
            it->_conf.push_back(tmp);
            break ;
          }
          ++it;
        }
        // add new server with config
        if (it == servers.end())
        {
          server._conf.push_back(tmp);
          servers.push_back(server);
        }
        // clear
        server._conf.clear();
        tmp.clear();
        context.clear();
      }
      else
        throw(Config::InvalidConfigFileException(nb_line));
    }
    else if (line[0])
      throw(Config::InvalidConfigFileException(nb_line));
  }
}

/**
 *		@brief	config 파일 block 내부에서 데이터 파싱
 *
 *		@param1 buffer: config file
 *		@param2 context: key value
 *		@param3 prec: old read line
 *		@param4 nb_line: read line number
 *		@param5 config:   map<std::string, map<std::string,std::string> >
 */
void			Config::getContent(std::string &buffer, std::string &context, std::string prec, size_t &nb_line, config &config)
{
  std::string			line;
  std::string			key;
  std::string			value;
  size_t			  	pos;
  size_t			  	tmp;

  // setting key value in context
  prec.pop_back();
  while (prec.back() == ' ' || prec.back() == '\t')
    prec.pop_back();
  context += prec + "|";
  while (ft::isspace(line[0]))
  {
    std::cout << "in isspace line[0] ?? " << std::endl;
    line.erase(line.begin());
  }
  // get config data
  while (line != "}" && !buffer.empty())
  {
    ft::getline(buffer, line);
    nb_line++;
    line = ft::trim(line);
    if (line[0] != '}')
    {
      pos = 0;
      // get elmt key, value
      while (line[pos] && line[pos] != ';' && line[pos] != '{')
      {
        while (line[pos] && !ft::isspace(line[pos]))
          key += line[pos++];
        while (ft::isspace(line[pos]))
          pos++;
        while (line[pos] && line[pos] != ';' && line[pos] != '{')
          value += line[pos++];
      }
      tmp = 0;
      if (line[pos] != ';' && line[pos] != '{')
        throw(Config::InvalidConfigFileException(nb_line));
      else
        tmp++;
      while (ft::isspace(line[pos + tmp]))
        tmp++;
      if (line[pos + tmp])
        throw(Config::InvalidConfigFileException(nb_line));
      else if (line[pos] == '{') // get location block data
        getContent(buffer, context, line, nb_line, config);
      else  // setting data in map
      {
        key = ft::trim(key);
        value = ft::trim(value);
        std::pair<std::string, std::string>	tmp(key, value);
        config[context].insert(tmp);
        key.clear();
        value.clear();
      }

    }
    else if (line[0] == '}' && !buffer.empty())
    { // key reset
      pos = 0;
      while (ft::isspace(line[1]))
        line.erase(line.begin() + 1);
      if (line[1])
        throw(Config::InvalidConfigFileException(nb_line));
      context.pop_back();
      context = context.substr(0, context.find_last_of('|') + 1);
    }
  }
  if (line[0] != '}')
    throw(Config::InvalidConfigFileException(nb_line));
}

/**
 *		init server, fd_set
 */
void			Config::init(fd_set *rSet, fd_set *wSet, fd_set *readSet, fd_set *writeSet, struct timeval *timeout)
{
  signal(SIGINT, exit);
  ft::FT_FD_ZERO(rSet);
  ft::FT_FD_ZERO(wSet);
  ft::FT_FD_ZERO(readSet);
  ft::FT_FD_ZERO(writeSet);
  timeout->tv_sec = 1;
  timeout->tv_usec = 0;

  for (std::vector<Server>::iterator it(g_servers.begin()); it != g_servers.end(); ++it)
    it->init(rSet, wSet);
}

/**
 *		Exception
 */
Config::InvalidConfigFileException::InvalidConfigFileException(void) {this->line = 0;}

Config::InvalidConfigFileException::InvalidConfigFileException(size_t d) {
  this->line = d;
  this->error = "line " + std::to_string(this->line) + ": Invalid Config File";
}

Config::InvalidConfigFileException::~InvalidConfigFileException(void) throw() {}

const char					*Config::InvalidConfigFileException::what(void) const throw()
{
  if (this->line)
    return (error.c_str());
  return ("Invalid Config File");
}
