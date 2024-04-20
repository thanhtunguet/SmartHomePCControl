using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Logging;
using System;
using System.Threading.Tasks;
using dotenv.net;

DotEnv.Load();

var builder = WebApplication.CreateBuilder(args);

// Add services to the container.
builder.Services.AddControllers();
// Learn more about configuring Swagger/OpenAPI at https://aka.ms/aspnetcore/swashbuckle
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();

var app = builder.Build();

// Configure the HTTP request pipeline.
if (app.Environment.IsDevelopment())
{
    app.UseSwagger();
    app.UseSwaggerUI();
}

app.UseHttpsRedirection();

app.UseRequestLogging();
app.UseResponseLogging();

app.UseAuthorization();

app.MapControllers();

app.Run();


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

public static class MiddlewareExtensions
{
    public static IApplicationBuilder UseRequestLogging(this IApplicationBuilder builder)
    {
        return builder.UseMiddleware<RequestLoggingMiddleware>();
    }

    public static IApplicationBuilder UseResponseLogging(this IApplicationBuilder builder)
    {
        return builder.UseMiddleware<ResponseLoggingMiddleware>();
    }
}

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
