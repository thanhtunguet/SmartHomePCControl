using Microsoft.AspNetCore.Mvc;
using System.Net;

namespace SmartHomePCControl.Controllers;

[ApiController]
[Route("/")]
public class AuthController : ControllerBase
{
    private readonly ILogger<AuthController> _logger;

    public AuthController(ILogger<AuthController> logger)
    {
        _logger = logger;
    }

    [HttpGet("login")]
    public IActionResult Login(string response_url)
    {
        return Content($@"
<!DOCTYPE html>
<html lang=""en"">
<head>
<title>Google Smart Home</title>
<meta charset=""utf-8"">
<meta name=""viewport"" content=""width=device-width, initial-scale=1"">
<link href=""https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css"" rel=""stylesheet"" integrity=""sha384-QWTKZyjpPEjISv5WaRU9OFeRpok6YctnYmDr5pNlyT2bRjXh0JMhjY6hW+ALEwIH"" crossorigin=""anonymous"">
</head>
<body>
<div class=""container my-2"">
<form action=""/login"" method=""post"">
<input type=""hidden"" name=""response_url"" value=""{WebUtility.HtmlEncode(response_url)}"" />
<button type=""submit"" class=""btn btn-primary"">Link this service to Google</button>
</form>
</div>
</body>
</html>", "text/html");
    }

    [HttpPost("login")]
    public IActionResult PostLogin([FromForm] string response_url)
    {
        _logger.LogInformation("/login", response_url);
        var decodedResponseUrl = WebUtility.UrlDecode(response_url);
        _logger.LogInformation("redirect:", decodedResponseUrl);
        return Redirect(decodedResponseUrl);
    }

    [HttpGet("fakeauth")]
    public IActionResult FakeAuth(string redirect_uri, string state)
    {
        _logger.LogInformation("/fakeauth", redirect_uri, state);
        var responseUrl = string.Format("{0}?code={1}&state={2}", WebUtility.UrlDecode(redirect_uri), "xxxxxx", state);
        var redirectUrl = $"/login?" +
                          $"response_url" +
                          $"={WebUtility.UrlEncode(responseUrl)}";
        _logger.LogInformation("redirect:", redirectUrl);
        return Redirect(redirectUrl);
    }

    [HttpPost("faketoken")]
    public IActionResult FakeToken([FromForm] Dictionary<string, string> body)
    {
        var grant_type = body["grant_type"];
        _logger.LogInformation("/faketoken", grant_type, body);
        var secondsInDay = 86400; // 60 * 60 * 24
        var token = new Dictionary<string, object>();
        var tokenType = "bearer";

        if (grant_type == "authorization_code")
        {
            token = new Dictionary<string, object>
            {
                { "token_type", tokenType },
                { "access_token", "123access" },
                { "refresh_token", "123refresh" },
                { "expires_in", secondsInDay }
            };
        }
        else if (grant_type == "refresh_token")
        {
            token = new Dictionary<string, object>
            {
                { "token_type", tokenType },
                { "access_token", "123access" },
                { "expires_in", secondsInDay }
            };
        }

        _logger.LogInformation("Token={0}\n", token);
        return Ok(token);
    }
}