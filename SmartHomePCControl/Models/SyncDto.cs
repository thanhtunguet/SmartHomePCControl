namespace SmartHomePCControl.Models;

public class SyncRequestDto : RequestDto
{
}

public class SyncResponseDto : ResponseDto
{
    public SyncResponsePayload payload { get; set; }
}

public class SyncResponsePayload : ResponsePayload
{
    public string? agentUserId { get; set; }

    public Device[] devices { get; set; }
}