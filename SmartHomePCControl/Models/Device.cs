using System.Text.Json.Serialization;

namespace SmartHomePCControl.Models;

public class Device
{
    public string id { get; set; }

    public string type { get; set; }
    public string[] traits { get; set; }

    public DeviceName name { get; set; }

    public bool willReportState { get; set; }

    public DeviceInfo deviceInfo { get; set; }

    public object customData { get; set; }

    public DeviceAttributes attributes { get; set; }
}