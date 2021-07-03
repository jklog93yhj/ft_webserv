#include "package.hpp"

// execve할때 CGI env 필요하므로 파싱
char		**Dispatcher::setCGIEnv(Client &client)
{
    char											**env;
    std::map<std::string, std::string> 				env_tmp;
    size_t											pos;

	env_tmp["CONTENT_LENGTH"] = std::to_string(client.req.body.size());
    if (client.req.headers.find("Content-Type") != client.req.headers.end())
        env_tmp["CONTENT_TYPE"] = client.req.headers["Content-Type"];
    env_tmp["GATEWAY_INTERFACE"] = "CGI/1.1";
    env_tmp["PATH_INFO"] = client.req.uri;
    env_tmp["PATH_TRANSLATED"] = client.conf["path"];
    if (client.req.uri.find('?') != std::string::npos)
        env_tmp["QUERY_STRING"] = client.req.uri.substr(client.req.uri.find('?') + 1);
    else
        env_tmp["QUERY_STRING"];
    env_tmp["REMOTE_ADDR"] = client.ip;
    env_tmp["REQUEST_METHOD"] = client.req.method;
    env_tmp["REQUEST_URI"] = client.req.uri;
    if (client.conf.find("exec") != client.conf.end())
        env_tmp["SCRIPT_NAME"] = client.conf["exec"];
    else
        env_tmp["SCRIPT_NAME"] = client.conf["path"];
    if (client.conf["listen"].find(":") != std::string::npos)
    {
        env_tmp["SERVER_NAME"] = client.conf["listen"].substr(0, client.conf["listen"].find(":"));
        env_tmp["SERVER_PORT"] = client.conf["listen"].substr(client.conf["listen"].find(":") + 1);
    }
    else
        env_tmp["SERVER_PORT"] = client.conf["listen"];
    env_tmp["SERVER_PROTOCOL"] = "HTTP/1.1";
    env_tmp["SERVER_SOFTWARE"] = "webserv";
    if (client.req.headers.find("Authorization") != client.req.headers.end())
    {
        pos = client.req.headers["Authorization"].find(" ");
        env_tmp["AUTH_TYPE"] = client.req.headers["Authorization"].substr(0, pos);
        env_tmp["REMOTE_USER"] = client.req.headers["Authorization"].substr(pos + 1);
        env_tmp["REMOTE_IDENT"] = client.req.headers["Authorization"].substr(pos + 1);
    }
    if (client.conf.find("php") != client.conf.end() && client.req.uri.find(".php") != std::string::npos)
        env_tmp["REDIRECT_STATUS"] = "200";
    std::map<std::string, std::string>::iterator b = client.req.headers.begin();
    while (b != client.req.headers.end())
    {
        env_tmp["HTTP_" + b->first] = b->second;
        ++b;
    }
    env = (char **)malloc(sizeof(char *) * (env_tmp.size() + 1));
    std::map<std::string, std::string>::iterator it = env_tmp.begin();
    int i = 0;
    while (it != env_tmp.end())
    {
        env[i] = strdup((it->first + "=" + it->second).c_str());
        ++i;
        ++it;
    }
    env[i] = NULL;
	/*
	std::cout << "AUTH_TYPE = ";
	std::cout << env_tmp["AUTH_TYPE"] << std::endl;
	std::cout << "CONTENT_LENGTH = ";
	std::cout << env_tmp["CONTENT_LENGTH"] << std::endl;
	std::cout << "CONTENT_TYPE = ";
	std::cout << env_tmp["CONTENT_TYPE"] << std::endl;
	std::cout << "GATEWAY_INTERFACE = ";
	std::cout << env_tmp["GATEWAY_INTERFACE"] << std::endl;
	std::cout << "PATH_INFO = ";
	std::cout << env_tmp["PATH_INFO"] << std::endl;
	std::cout << "PATH_TRANSLATED = ";
	std::cout << env_tmp["PATH_TRANSLATED"] << std::endl;
	std::cout << "QUERY_STRING = ";
	std::cout << env_tmp["QUERY_STRING"] << std::endl;
	std::cout << "REMOTE_ADDR = ";
	std::cout << env_tmp["REMOTE_ADDR"] << std::endl;
	std::cout << "REMOTE_IDENT = ";
	std::cout << env_tmp["REMOTE_IDENT"] << std::endl;
	std::cout << "REMOTE_USER = ";
	std::cout << env_tmp["REMOTE_USER"] << std::endl;
	std::cout << "REQUEST_METHOD = ";
	std::cout << env_tmp["REQUEST_METHOD"] << std::endl;
	std::cout << "REQUEST_URI = ";
	std::cout << env_tmp["REQUEST_URI"] << std::endl;
	std::cout << "SCRIPT_NAME = ";
	std::cout << env_tmp["SCRIPT_NAME"] << std::endl;
	std::cout << "SERVER_NAME = ";
	std::cout << env_tmp["SERVER_NAME"] << std::endl;
	std::cout << "SERVER_PORT = ";
	std::cout << env_tmp["SERVER_PORT"] << std::endl;
	std::cout << "SERVER_PROTOCOL = ";
	std::cout << env_tmp["SERVER_PROTOCOL"] << std::endl;
	std::cout << "SERVER_SOFTWARE = ";
	std::cout << env_tmp["SERVER_SOFTWARE"] << std::endl;
	*/
    return (env);
}

void		Dispatcher::executeCGI(Client &client)
{
    char			**args = NULL;
    char			**env = NULL;
    std::string		path;
    int				ret;
    int				tubes[2];

	//bin/ls /User/hwyu/asd
    if (client.conf["php"][0] && client.conf["path"].find(".php") != std::string::npos)
        path = client.conf["php"];
	// conf안에 exec 뒷부분
    else if (client.conf["exec"][0])
        path = client.conf["exec"];
	///Users/hwyu/Desktop/hwyu_webserv3/www/YoupiBanane/.bla => .bla가 붙음
    else
        path = client.conf["path"];
    close(client.read_fd);
    client.read_fd = -1;
    args = (char **)(malloc(sizeof(char *) * 3));
    args[0] = strdup(path.c_str());
    args[1] = strdup(client.conf["path"].c_str());
    args[2] = NULL;
    env = setCGIEnv(client);
	//TMP_PATH = /tmp/cgi.tmp
    client.tmp_fd = open(TMP_PATH, O_WRONLY | O_CREAT, 0666);
    pipe(tubes);
    g_logger.log("executing CGI for " + client.ip + ":" + std::to_string(client.port), MED);
    if ((client.cgi_pid = fork()) == 0)
    {
        close(tubes[1]);
		dup2(tubes[0], 0); // stdin
        dup2(client.tmp_fd, 1); //stdout
        errno = 0;
        ret = execve(path.c_str(), args, env);
        if (ret == -1)
        {
            std::cerr << "Error with CGI: " << strerror(errno) << std::endl;
            exit(1);
        }
    }
    else
    {
        close(tubes[0]);
        client.write_fd = tubes[1];
        client.read_fd = open(TMP_PATH, O_RDONLY);
        client.setFileToWrite(true);
    }
    ft::freeAll(args, env);
}
