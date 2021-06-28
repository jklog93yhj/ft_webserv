#include "package.hpp"

int			Dispatcher::setStatusCode(Client &client)
{
    std::string                 credential;
    int							ret;
	std::cout << "[ setStatusCode ]" << std::endl;

    if (client.req.method != "CONNECT"
        && client.req.method != "TRACE"
        && client.req.method != "OPTIONS")
    {
		/*
		 *
		HTTP/1.1 200 OK
		Content-Length: 398
		Content-Type: text/html
		Date: Fri, 25 Jun 2021 15:46:41 KST
		Last-Modified: Mon, 21 Jun 2
		 *
		 */
        client.res.version = "HTTP/1.1";
        client.res.status_code = OK;
		//찾는 문자열이 없는 경우 npos 리턴
		// get에서 status_code를 처리할때 봐야될 것
		// auth(config파일), Authorization, Basic
		// config파일에 auth 가 있나 없나를 체크
        if (client.conf["methods"].find(client.req.method) == std::string::npos)
            client.res.status_code = NOTALLOWED;
        else if (client.conf.find("auth") != client.conf.end())
        {
            client.res.status_code = UNAUTHORIZED;
            if (client.req.headers.find("Authorization") != client.req.headers.end())
            {
                std::string &str = client.req.headers["Authorization"];
                if (str.find("Basic") != std::string::npos)
                    str = str.substr(str.find("Basic ") + 6);
                credential = decode64(client.req.headers["Authorization"].c_str());
                if (credential == client.conf["auth"])
                    client.res.status_code = OK;
            }
        }
    }
	// GETHEADStatus로 넘어감
    ret = (this->*status[client.req.method])(client);
	std::cout << "ret = " << ret << std::endl;
    if (ret == 0)
        getErrorPage(client);
    return (ret);
}

int			Dispatcher::GETHEADStatus(Client &client)
{
    struct stat		info;

    if (client.res.status_code == OK)
    {
        errno = 0;
		std::cout << "client path = " << client.conf["path"].c_str() << std::endl;
		// path = Users/hwyu/Desktop/hwyu_webserv3/www/content/oldindex.html
        client.read_fd = open(client.conf["path"].c_str(), O_RDONLY);
        if (client.read_fd == -1 && errno == ENOENT)
            client.res.status_code = NOTFOUND;
        else if (client.read_fd == -1)
            client.res.status_code = INTERNALERROR;
        else
        {
            fstat(client.read_fd, &info);
			// listing 검사를 왜하는지?
            if (!S_ISDIR(info.st_mode)
                || (S_ISDIR(info.st_mode) && client.conf["listing"] == "on"))
                return (1);
            else
                client.res.status_code = NOTFOUND;
        }
    }
    return (0);
}

int			Dispatcher::POSTStatus(Client &client)
{
    int				fd;
    struct stat		info;

    if (client.res.status_code == OK && client.conf.find("max_body") != client.conf.end()
        && client.req.body.size() > (unsigned long)atoi(client.conf["max_body"].c_str()))
        client.res.status_code = REQTOOLARGE;
    if (client.res.status_code == OK)
    {
        if (client.conf.find("CGI") != client.conf.end()
            && client.req.uri.find(client.conf["CGI"]) != std::string::npos)
        {
            if (client.conf["exec"][0])
                client.read_fd = open(client.conf["exec"].c_str(), O_RDONLY);
            else
                client.read_fd = open(client.conf["path"].c_str(), O_RDONLY);
            fstat(client.read_fd, &info);
            if (client.read_fd == -1 || S_ISDIR(info.st_mode))
                client.res.status_code = INTERNALERROR;
            else
                return (1);
        }
        else
        {
            errno = 0;
            fd = open(client.conf["path"].c_str(), O_RDONLY);
            if (fd == -1 && errno == ENOENT)
                client.res.status_code = CREATED;
            else if (fd != -1)
                client.res.status_code = OK;
            close(fd);
            client.write_fd = open(client.conf["path"].c_str(), O_WRONLY | O_APPEND | O_CREAT, 0666);
            if (client.write_fd == -1)
                client.res.status_code = INTERNALERROR;
            else
                return (1);
        }
    }
    return (0);
}

int			Dispatcher::PUTStatus(Client &client)
{
    int 		fd;
    struct stat	info;
    int			save_err;


    if (client.res.status_code == OK && client.conf.find("max_body") != client.conf.end()
        && client.req.body.size() > (unsigned long)atoi(client.conf["max_body"].c_str()))
        client.res.status_code = REQTOOLARGE;
    else if (client.res.status_code == OK)
    {
        errno = 0;
        fd = open(client.conf["path"].c_str(), O_RDONLY);
        save_err = errno;
        fstat(fd, &info);
        if (S_ISDIR(info.st_mode))
            client.res.status_code = NOTFOUND;
        else
        {
            if (fd == -1 && save_err == ENOENT)
                client.res.status_code = CREATED;
            else if (fd == -1)
            {
                client.res.status_code = INTERNALERROR;
                return (0);
            }
            else
            {
                client.res.status_code = NOCONTENT;
                if (close(fd) == -1)
                {
                    client.res.status_code = INTERNALERROR;
                    return (0);
                }
            }
            client.write_fd = open(client.conf["path"].c_str(), O_WRONLY | O_CREAT, 0666);
            if (client.write_fd == -1)
            {
                client.res.status_code = INTERNALERROR;
                return (0);
            }
            return (1);
        }
    }
    return (0);
}

int			Dispatcher::CONNECTStatus(Client &client)
{
    client.res.version = "HTTP/1.1";
    client.res.status_code = NOTIMPLEMENTED;
    return (0);
}

int			Dispatcher::TRACEStatus(Client &client)
{
    client.res.version = "HTTP/1.1";
    if (client.conf["methods"].find(client.req.method) == std::string::npos)
    {
        client.res.status_code = NOTALLOWED;
        return (0);
    }
    else
    {
        client.res.status_code = OK;
        return (1);
    }
}

int			Dispatcher::OPTIONSStatus(Client &client)
{
    client.res.version = "HTTP/1.1";
    client.res.status_code = NOCONTENT;
    return (1);
}

int			Dispatcher::DELETEStatus(Client &client)
{
    int 		fd;
    struct stat	info;
    int			save_err;

    if (client.res.status_code == OK)
    {
        errno = 0;
        fd = open(client.conf["path"].c_str(), O_RDONLY);
        save_err = errno;
        fstat(fd, &info);
        if ((fd == -1 && save_err == ENOENT) || S_ISDIR(info.st_mode))
            client.res.status_code = NOTFOUND;
        else if (fd == -1)
            client.res.status_code = INTERNALERROR;
        else
            return (1);
    }
    return (0);
}
