#include "package.hpp"

int			Dispatcher::setStatusCode(Client &client)
{
    std::string                 credential;
    int							ret;

    // 메소드가 connect, trace, option이 아닐 때
    if (client.req.method != "CONNECT"
        && client.req.method != "TRACE"
        && client.req.method != "OPTIONS")
    {
		/*
        HTTP/1.1 200 OK
		Content-Length: 398
		Content-Type: text/html
		Date: Fri, 25 Jun 2021 15:46:41 KST
		Last-Modified: Mon, 21 Jun 2
        */
		// conf에서 처리할 수 있는 메소드 중에 request로 들어온 메소드가 없는 경우:
        // (예외처리 용도 정도로 사용하는 것 같은데, 실제로 이 method에 걸리는 경우의 수는 찾지 못했음.
        // <- 애초에 GET, PUT 등 맞는 경우에 타고 들어와서 status code를 설정하기 때문)
        client.res.version = "HTTP/1.1";
        client.res.status_code = OK;
		//찾는 문자열이 없는 경우 npos 리턴
		// get에서 status_code를 처리할때 봐야될 것
		// auth(config파일), Authorization, Basic
		// config파일에 auth 가 있나 없나를 체크
        if (client.conf["methods"].find(client.req.method) == std::string::npos)
            client.res.status_code = NOTALLOWED; // 405 Method Not Allowed

        // 혹은, conf에 auth 필드가 있는 경우 == 권한이 있어야 접속할 수 있다.
        // <- 기본인증 : 웹사이트에 접근 권한이 있는지를 확인하는 작업이다.
        // Client가 Request에서 Authorization 필드에 인증키를 보내서 주면, 우리는 이를 검증하는 방식으로 이루어진다.
        // 검증이 완료되면 200 OK를 반환
        else if (client.conf.find("auth") != client.conf.end())
        {
            client.res.status_code = UNAUTHORIZED;
            if (client.req.headers.find("Authorization") != client.req.headers.end())
            {
                // 아직 인증 전이기 때문에 401 권한오류 코드를 일단 담는다.
                std::string &str = client.req.headers["Authorization"];
                // 여기서 Basic은 기본 인증을 의미한다.
                if (str.find("Basic") != std::string::npos)
                // Client가 인증을 위해 보내는 파라미터의 예시를 보면 다음과 같다.
                // Authorization: Basic YWSSwe3KJFs52akdjQ=
                // 우리가 여기서 필요한 값은 "YWSSwe3KJFs52akdjQ=" 뿐이므로 Basic 뒤만 str에 저장한다.
                    str = str.substr(str.find("Basic ") + 6);
                credential = decode64(client.req.headers["Authorization"].c_str());
                // credential 은 client가 보낸 파라미터를 디코딩한 결과값이다.
                if (credential == client.conf["auth"])
                    client.res.status_code = OK;
                    // 만약에 같지 않으면 그대로 401 오류 상태이다.
            }
        }
    }
	// GETHEADStatus로 넘어감
    // ret 은 GETHEADStatus에서 status_code 가 OK가 아니면 0을 반환한다.
    ret = (this->*status[client.req.method])(client);

    // OK가 아닌 경우 error page 발송
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
		// path = Users/hwyu/Desktop/hwyu_webserv3/www/content/oldindex.html
        client.read_fd = open(client.conf["path"].c_str(), O_RDONLY);
        if (client.read_fd == -1 && errno == ENOENT)
            client.res.status_code = NOTFOUND;
        else if (client.read_fd == -1)
            client.res.status_code = INTERNALERROR;
        else
        {
            fstat(client.read_fd, &info);
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
