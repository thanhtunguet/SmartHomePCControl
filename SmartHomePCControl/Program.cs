using dotenv.net;
using SmartHomePCControl.Middleware;
using SmartHomePCControl.Services;

DotEnv.Load();

var builder = WebApplication.CreateBuilder(args);

// Add services to the container.
builder.Services.AddControllers();
// Learn more about configuring Swagger/OpenAPI at https://aka.ms/aspnetcore/swashbuckle
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();
builder.Services.AddScoped<WakeOnLanService>();

var app = builder.Build();

app.UseSwagger();
app.UseSwaggerUI();

app.UseHttpsRedirection();

app.UseRequestLogging();
app.UseResponseLogging();

app.UseAuthorization();

app.MapControllers();
app.UseCors();

app.Run();