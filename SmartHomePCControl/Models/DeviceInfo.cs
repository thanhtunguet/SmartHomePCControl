using System.Text.Json.Serialization;

namespace SmartHomePCControl.Models;

public class DeviceInfo
{
    [JsonPropertyName("manufacturer")]
    public string Manufacturer { get; set; }

    [JsonPropertyName("model")]
    public string Model { get; set; }

    [JsonPropertyName("hwVersion")]
    public string HwVersion { get; set; }

    [JsonPropertyName("swVersion")]
    public string SwVersion { get; set; }
}