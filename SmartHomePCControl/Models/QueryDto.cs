namespace SmartHomePCControl.Models;

public class QueryRequestDto : RequestDto
{
    public QueryInputPayload payload { get; set; }
}

public class QueryInputPayload : RequestDtoInputPayload
{
    public Device[] devices { get; set; }
}

public class QueryResponseDto : ResponseDto
{
    public QueryResponsePayload payload { get; set; }
}

public class QueryResponsePayload : ResponsePayload
{
    public Dictionary<string, DeviceAttributes> devices { get; set; }
}