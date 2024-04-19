using System.Collections.Generic;

namespace SmartHomePCControl.Models;

public class RequestDto
{
    public string requestId { get; set; }

    public string? agentUserId { get; set; }

    public RequestDtoInput[] inputs { get; set; }
}

public class RequestDtoInput
{
    public string intent { get; set; }

    public RequestDtoInputPayload? payload { get; set; }
}

public class RequestDtoInputPayload
{
}

public class ResponseDto
{
    public string requestId { get; set; }

    public ResponsePayload payload { get; set; }
}

public class ResponsePayload
{
}