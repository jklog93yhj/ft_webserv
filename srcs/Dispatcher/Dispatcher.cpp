#include "package.hpp"

Dispatcher* Dispatcher::instance = NULL;

Dispatcher::Dispatcher()
{
    method["GET"] = &Dispatcher::GETHEADMethod;
    method["HEAD"] = &Dispatcher::GETHEADMethod;
    method["PUT"] = &Dispatcher::PUTMethod;
    method["POST"] = &Dispatcher::POSTMethod;
    method["CONNECT"] = &Dispatcher::CONNECTMethod;
    method["TRACE"] = &Dispatcher::TRACEMethod;
    method["OPTIONS"] = &Dispatcher::OPTIONSMethod;
    method["DELETE"] = &Dispatcher::DELETEMethod;
    method["BAD"] = &Dispatcher::handlingBadRequest;

    status["GET"] = &Dispatcher::GETHEADStatus;
    status["HEAD"] = &Dispatcher::GETHEADStatus;
    status["PUT"] = &Dispatcher::PUTStatus;
    status["POST"] = &Dispatcher::POSTStatus;
    status["CONNECT"] = &Dispatcher::CONNECTStatus;
    status["TRACE"] = &Dispatcher::TRACEStatus;
    status["OPTIONS"] = &Dispatcher::OPTIONSStatus;
    status["DELETE"] = &Dispatcher::DELETEStatus;

    MIMETypes[".txt"] = "text/plain";
    MIMETypes[".bin"] = "application/octet-stream";
    MIMETypes[".jpeg"] = "image/jpeg";
    MIMETypes[".jpg"] = "image/jpeg";
    MIMETypes[".html"] = "text/html";
    MIMETypes[".htm"] = "text/html";
    MIMETypes[".png"] = "image/png";
    MIMETypes[".bmp"] = "image/bmp";
    MIMETypes[".pdf"] = "application/pdf";
    MIMETypes[".tar"] = "application/x-tar";
    MIMETypes[".json"] = "application/json";
    MIMETypes[".css"] = "text/css";
    MIMETypes[".js"] = "application/javascript";
    MIMETypes[".mp3"] = "audio/mpeg";
    MIMETypes[".avi"] = "video/x-msvideo";
}

void    Dispatcher::execute(Client &client)
{
	//이쪽에서 위에 정의된 것을 바탕으로 함수 찾아서 들어감
	// ex) method = GET이면 GETHEADMethod로 감
    (this->*method[client.req.method])(client);
}


// code -> header -> body -> body
void	Dispatcher::GETHEADMethod(Client &client)
{
	// 파일 불러올때 그 정보 담기위한 곳
    struct stat	file_info;

    switch (client.status)
    {
        case Client::CODE:
            setStatusCode(client);

            /* fstat함수는 stat, lstat와 첫 번째
			 * 인자가 다른데, fstat함수는 첫번째
			 * 인자로 파일 디스크립터 번호를 인자로
			 * 받고 stat와 동일한 기능을 수행한다.
			*/
            fstat(client.read_fd, &file_info);

            // S_ISDIR = 디렉토리 파일인지 확인
			// file_info.st_mode = 33188
			// client.conf["listing"] == NULL
            // fd를 열었을 때, 그 path의 상태가 dir 이고, listing 이 활성화 되어있을 떄, listing 모드로 들어간다.
            if (S_ISDIR(file_info.st_mode) && client.conf["listing"] == "on")
                createListing(client);

            // status_code = 200 OK
            // Not Found 상태이면, Negotiate로 들어간다.
            if (client.res.status_code == NOTFOUND)
                negotiate(client);

			//std::cout << "status_code = " << client.res.status_code << std::endl;
            if (checkCGI(client) && client.res.status_code == OK)
            {
                executeCGI(client);
                client.status = Client::CGI;
            }

            // 이런 예외사항들이 아닌 모든 default 일때, Header부터 읽기 시작한다.
            else
                client.status = Client::HEADERS;
            client.setFileToRead(true);
            break ;
        case Client::CGI:
			// read값 바뀌기전까지 대기
            if (client.read_fd == -1)
            {
                _parser.parseCGIResult(client);
                client.status = Client::HEADERS;
            }
            break ;

        // default로 들어가서, Header 부터 읽기 시작할 때:
        // ---------------------------------------------
        // Content-Length: 398
        // Content-Type: text/html
        // Date: Tue, 29 Jun 2021 20:50:52 KST
        // Last-Modified: Tue, 29 Jun 2021 15:32:09 KST
        // Server: webserv
        // ----------------------------------------------
        case Client::HEADERS:
			// 파일정보 읽기
            lstat(client.conf["path"].c_str(), &file_info);
            // Directory가 아니면 -> file일때, txt, html, jpg, ... => last-modified는 필수로 들어간다.

			// 디렉토리가 아니면
            // content-type이 존재하지 않으면,
            if (!S_ISDIR(file_info.st_mode))
                client.res.headers["Last-Modified"] = getLastModified(client.conf["path"]);
			// content-type이 존재하지 않으면,type 세팅
            if (client.res.headers["Content-Type"][0] == '\0')
                client.res.headers["Content-Type"] = findType(client);
            // 401 오류 상태이면, Basic으로 인증요청을 할 것이라는 것을 response를 통해 알려준다.
            if (client.res.status_code == UNAUTHORIZED)
                client.res.headers["WWW-Authenticate"] = "Basic";

            // https://developer.mozilla.org/ko/docs/Web/HTTP/Headers/Allow
            // NOTALLOWED(405 Method Not Allowed) 상태
            else if (client.res.status_code == NOTALLOWED)
                client.res.headers["Allow"] = client.conf["methods"];
            client.res.headers["Date"] = ft::getDate();
            client.res.headers["Server"] = "webserv";
            client.status = Client::BODY;
            break ;
        case Client::BODY:
			// 끝까지 다  읽은 경우?
			// read_fd = 1
            if (client.read_fd == -1)
            {
                client.res.headers["Content-Length"] = std::to_string(client.res.body.size());
				//지금까지 파싱한 것들 string으로 병합
                createResponse(client);
                client.status = Client::RESPONSE;
            }
            break;
    }
}

void	Dispatcher::POSTMethod(Client &client)
{
    switch (client.status)
    {
        case Client::BODYPARSING:
            _parser.parseBody(client);
            break ;
        case Client::CODE:
            // setStatusCode와 notion [Basic authentication] 참조
            setStatusCode(client);
            if (checkCGI(client) && client.res.status_code == OK)
            {
                executeCGI(client);
                client.status = Client::CGI;
                client.setFileToRead(true);
            }
            else
            {
                // 201 = 200과 동일한데, 성공과 동시에 새로운 리소스가 생성되었다는 의미를 포함한다.
                if (client.res.status_code == OK || client.res.status_code == CREATED)
                    client.setFileToWrite(true);
                else
                // 에러 메시지
                    client.setFileToRead(true);
                client.status = Client::HEADERS;
            }
            break ;
        case Client::CGI:
			// fork 하고 오는 동안 무한루프 돌면서 대기
            if (client.read_fd == -1)
            {
                _parser.parseCGIResult(client);
                client.status = Client::HEADERS;
            }
            break ;
        case Client::HEADERS:
        // 이 부분이 AUTH 요청을 보내는 부분.
        // WWW-Authenticat: Basic 은 기본 인증 프로토콜을 사용하겠다는 의미.
            if (client.res.status_code == UNAUTHORIZED)
                client.res.headers["WWW-Authenticate"] = "Basic";
            else if (client.res.status_code == NOTALLOWED)
                client.res.headers["Allow"] = client.conf["methods"];
            else if (client.res.status_code == CREATED)
                client.res.body = "File created\n";
            else if (client.res.status_code == OK && !checkCGI(client))
                client.res.body = "File modified\n";
            client.res.headers["Date"] = ft::getDate();
            client.res.headers["Server"] = "webserv";
            client.status = Client::BODY;
            break ;
        case Client::BODY:
            if (client.read_fd == -1 && client.write_fd == -1)
            {
                if (client.res.headers["Content-Length"][0] == '\0')
                    client.res.headers["Content-Length"] = std::to_string(client.res.body.size());
                createResponse(client);
                client.status = Client::RESPONSE;
            }
            break ;
    }
}

void	Dispatcher::PUTMethod(Client &client)
{
    std::string		path;
    std::string		body;

    switch (client.status)
    {
        case Client::BODYPARSING:
            _parser.parseBody(client);
            break ;
        case Client::CODE:
            // ERROR가 아닐 때 (200 OK일 때)
            if (setStatusCode(client))
                client.setFileToWrite(true);
            else
                client.setFileToRead(true);

            // HTTP 201 Created는 요청이 성공적으로 처리되었으며, 자원이 생성되었음을 나타내는 성공 상태 응답 코드입니다.
            // 해당 HTTP 요청에 대해 회신되기 이전에 정상적으로 생성된 자원은 회신 메시지의 본문(body)에 동봉되고,
            // 구체적으로는 요청 메시지의 URL이나, Location (en-US) 헤더의 내용에 위치하게 됩니다.
            if (client.res.status_code == CREATED || client.res.status_code == NOCONTENT)
            {
                client.res.headers["Location"] = client.req.uri;
                if (client.res.status_code == CREATED)
                    client.res.body = "Resource created\n";
            }
            else if (client.res.status_code == NOTALLOWED)
                client.res.headers["Allow"] = client.conf["methods"];
            else if (client.res.status_code == UNAUTHORIZED)
                client.res.headers["WWW-Authenticate"] = "Basic";
            client.res.headers["Date"] = ft::getDate();
            client.res.headers["Server"] = "webserv";
            client.status = Client::BODY;
            break ;
        case Client::BODY:
            if (client.write_fd == -1 && client.read_fd == -1)
            {
                client.res.headers["Content-Length"] = std::to_string(client.res.body.size());
                createResponse(client);
                client.status = Client::RESPONSE;
            }
            break ;
    }

}


void	Dispatcher::CONNECTMethod(Client &client)
{
    switch (client.status)
    {
        case Client::CODE:
            setStatusCode(client);
            client.setFileToRead(true);
            client.res.headers["Date"] = ft::getDate();
            client.res.headers["Server"] = "webserv";
            client.status = Client::BODY;
            break ;
        case Client::BODY:
            if (client.read_fd == -1)
            {
                client.res.headers["Content-Length"] = std::to_string(client.res.body.size());
                createResponse(client);
                client.status = Client::RESPONSE;
            }
            break ;
    }
}

void	Dispatcher::TRACEMethod(Client &client)
{
    switch (client.status)
    {
        case Client::CODE:
            setStatusCode(client);
            if (client.res.status_code == OK)
            {
                client.res.headers["Content-Type"] = "message/http";
                client.res.body = client.req.method + " " + client.req.uri + " " + client.req.version + "\r\n";
                for (std::map<std::string, std::string>::iterator it(client.req.headers.begin());
                     it != client.req.headers.end(); ++it)
                {
                    client.res.body += it->first + ": " + it->second + "\r\n";
                }
            }
            else
                client.setFileToRead(true);
            client.res.headers["Date"] = ft::getDate();
            client.res.headers["Server"] = "webserv";
            client.status = Client::BODY;
            break ;
        case Client::BODY:
            if (client.read_fd == -1)
            {
                client.res.headers["Content-Length"] = std::to_string(client.res.body.size());
                createResponse(client);
                client.status = Client::RESPONSE;
            }
            break ;
    }
}

void	Dispatcher::OPTIONSMethod(Client &client)
{
    switch (client.status)
    {
        case Client::CODE:
            setStatusCode(client);
            client.res.headers["Date"] = ft::getDate();
            client.res.headers["Server"] = "webserv";
            if (client.req.uri != "*")
                client.res.headers["Allow"] = client.conf["methods"];
            createResponse(client);
            client.status = Client::RESPONSE;
            break ;
    }
}

void	Dispatcher::DELETEMethod(Client &client)
{
    switch (client.status)
    {
        case Client::CODE:
            if (!setStatusCode(client))
                client.setFileToRead(true);
            if (client.res.status_code == OK)
            {
                unlink(client.conf["path"].c_str());
                client.res.body = "File deleted\n";
            }
            else if (client.res.status_code == NOTALLOWED)
                client.res.headers["Allow"] = client.conf["methods"];
            else if (client.res.status_code == UNAUTHORIZED)
                client.res.headers["WWW-Authenticate"] = "Basic";
            client.res.headers["Date"] = ft::getDate();
            client.res.headers["Server"] = "webserv";
            client.status = Client::BODY;
            break ;
        case Client::BODY:
            if (client.read_fd == -1)
            {
                client.res.headers["Content-Length"] = std::to_string(client.res.body.size());
                createResponse(client);
                client.status = Client::RESPONSE;
            }
            break ;
    }
}

void	Dispatcher::handlingBadRequest(Client &client)
{
    switch (client.status)
    {
        case Client::CODE:
            client.res.version = "HTTP/1.1";
            client.res.status_code = BADREQUEST;
            getErrorPage(client);
            client.setFileToRead(true);
            client.res.headers["Date"] = ft::getDate();
            client.res.headers["Server"] = "webserv";
            client.status = Client::BODY;
            break ;
        case Client::BODY:
            if (client.read_fd == -1)
            {
                client.res.headers["Content-Length"] = std::to_string(client.res.body.size());
                createResponse(client);
                client.status = Client::RESPONSE;
            }
            break ;
    }
}
