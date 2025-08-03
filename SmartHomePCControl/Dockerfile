FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
WORKDIR /app

# Copy everything
WORKDIR /src
COPY ["SmartHomePCControl.csproj", "./"]
RUN dotnet restore "SmartHomePCControl.csproj"
COPY . .
# Build and publish a release
RUN dotnet build "SmartHomePCControl.csproj" -c Release -o /app/build
RUN dotnet publish "SmartHomePCControl.csproj" -c Release -o /app/publish

# Build runtime image
FROM mcr.microsoft.com/dotnet/aspnet:8.0 as final
WORKDIR /app
COPY --from=build /app/publish .
ENTRYPOINT ["dotnet", "SmartHomePCControl.dll"]
