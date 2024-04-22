public class ResponseLoggingMiddleware
{
    private readonly RequestDelegate _next;
    private readonly ILogger<ResponseLoggingMiddleware> _logger;

    public ResponseLoggingMiddleware(RequestDelegate next, ILogger<ResponseLoggingMiddleware> logger)
    {
        _next = next;
        _logger = logger;
    }

    public async Task Invoke(HttpContext context)
    {
        // Capture the original response body stream
        var originalBodyStream = context.Response.Body;

        try
        {
            // Replace the response body stream with a MemoryStream
            using (var responseBodyStream = new MemoryStream())
            {
                context.Response.Body = responseBodyStream;

                // Call the next middleware in the pipeline
                await _next(context);

                // Rewind the MemoryStream to read its content
                responseBodyStream.Seek(0, SeekOrigin.Begin);

                // Read the response content from the MemoryStream
                var responseBody = await new StreamReader(responseBodyStream).ReadToEndAsync();

                // Log the response content to the console
                _logger.LogInformation($"Response content: {responseBody}");

                // Copy the response content back to the original response body stream
                responseBodyStream.Seek(0, SeekOrigin.Begin);
                await responseBodyStream.CopyToAsync(originalBodyStream);
            }
        }
        finally
        {
            // Restore the original response body stream
            context.Response.Body = originalBodyStream;
        }
    }
}