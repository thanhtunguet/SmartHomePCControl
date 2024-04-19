using System.Text.Json.Serialization;

namespace SmartHomePCControl.Models;

public class ExecuteRequestDto : RequestDto
{
    public ExecuteRequestInput[] inputs { get; set; }
}

public class ExecuteRequestInput : RequestDtoInput
{
    public ExecuteRequestInputPayload payload { get; set; }
}

public class ExecuteRequestInputPayload : RequestDtoInputPayload
{
    public Command[] commands { get; set; }
}

public class Command
{
    public Device[] devices { get; set; }

    public Execution[] execution { get; set; }
}

public class Execution
{
    public string command { get; set; }

    [JsonPropertyName("params")]
    public Params Params { get; set; }
}

public class Params
{
    public bool on { get; set; }
}

public class ExecuteResponseDto : ResponseDto
{
    public ExecuteResponsePayload payload { get; set; }
}

public class ExecuteResponsePayload : ResponsePayload
{
    public ExecuteResponseCommand[] commands { get; set; }
}

public class ExecuteResponseCommand
{
    public string[] ids { get; set; }
    
    public string status { get; set; }
    
    public DeviceAttributes states { get; set; }
}
