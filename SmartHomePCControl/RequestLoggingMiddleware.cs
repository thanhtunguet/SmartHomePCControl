public class RequestLoggingMiddleware
{
    private readonly RequestDelegate _next;
    private readonly ILogger<RequestLoggingMiddleware> _logger;

    public RequestLoggingMiddleware(RequestDelegate next, ILogger<RequestLoggingMiddleware> logger)
    {
        _next = next;
        _logger = logger;
    }

    public async Task Invoke(HttpContext context)
    {
        // Log the request URL and method to the console
        _logger.LogInformation(
            $"Request URL: {context.Request.Path}," +
            $"Method: {context.Request.Method}," +
            $"ContentType: {context.Request.ContentType}");

        // Call the next middleware in the pipeline
        await _next(context);
    }
}