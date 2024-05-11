namespace SmartHomePCControl.Middleware;

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
