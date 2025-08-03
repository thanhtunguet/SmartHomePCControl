using Microsoft.AspNetCore.Mvc;
using System.Text.Json;
using SmartHomePCControl.Models;
using SmartHomePCControl.Services;

namespace SmartHomePCControl.Controllers;

[ApiController]
public class GoogleHomeController : ControllerBase
{
    private readonly WakeOnLanService _wakeOnLanService;

        public GoogleHomeController(WakeOnLanService wakeOnLanService)
    {
        _wakeOnLanService = wakeOnLanService;
        _deviceMac = Environment.GetEnvironmentVariable("DEVICE_MAC") ?? throw new ArgumentNullException("DEVICE_MAC environment variable not set");
        _serverIp = Environment.GetEnvironmentVariable("SERVER_IP") ?? throw new ArgumentNullException("SERVER_IP environment variable not set");
    }

    private readonly string _deviceMac;
    private readonly string _serverIp;
    private const int ServerPort = 3389;
    private const int ServerShutdownPort = 10675;

    // Static instance of the device
    private static readonly Device PcDevice = new Device()
    {
        id = "MyPC",
        type = "action.devices.types.SWITCH",
        traits = new[] { DeviceTraits.OnOff },
        name = new DeviceName { name = "My PC" },
        willReportState = true,
        deviceInfo = new DeviceInfo
        {
            Manufacturer = "Asus",
            Model = "DesktopComputer",
            HwVersion = "Core i5 13400F",
            SwVersion = "Windows 11",
        }
    };

    [HttpGet(PcControllerRoute.TurnOn)]
    public async Task<IActionResult> HandleWakeUpDevice()
    {
        TurnOnMyPC();
        return await Task.FromResult(Ok());
    }

    [HttpGet(PcControllerRoute.TurnOff)]
    public async Task<IActionResult> HandleShutdownDevice()
    {
        TurnOffMyPC();
        return await Task.FromResult(Ok());
    }

    [HttpPost(PcControllerRoute.GoogleActions)]
    public async Task<IActionResult> HandleSmartHomeRequest([FromBody] JsonElement request)
    {
        string jsonString = request.ToString();
        RequestDto requestDto = JsonSerializer.Deserialize<RequestDto>(jsonString);

        if (requestDto == null || requestDto?.inputs == null || !requestDto.inputs.Any())
        {
            return BadRequest("Invalid request format");
        }

        string intent = requestDto.inputs[0].intent;

        switch (intent)
        {
            case DeviceAction.Sync:
                SyncRequestDto syncRequestDto = JsonSerializer.Deserialize<SyncRequestDto>(jsonString);
                return SyncDevices(syncRequestDto);

            case DeviceAction.Query:
                QueryRequestDto queryRequestDto = JsonSerializer.Deserialize<QueryRequestDto>(jsonString);
                return await QueryDevices(queryRequestDto);

            case DeviceAction.Execute:
                ExecuteRequestDto executeRequestDto = JsonSerializer.Deserialize<ExecuteRequestDto>(jsonString);
                return await ExecuteCommand(executeRequestDto);

            default:
                return BadRequest("Invalid intent");
        }
    }

    private IActionResult SyncDevices(RequestDto request)
    {
        SyncResponseDto response = new SyncResponseDto
        {
            requestId = request.requestId,
            payload = new SyncResponsePayload
            {
                agentUserId = request.agentUserId,
                devices = new[] { PcDevice }
            }
        };

        return Ok(response);
    }

    private async Task<IActionResult> QueryDevices(RequestDto request)
    {
        Dictionary<string, DeviceAttributes> devices = new Dictionary<string, DeviceAttributes>
        {
            {
                PcDevice.id,
                new DeviceAttributes
                {
                    status = QueryStatus.SUCCESS,
                    on = await _wakeOnLanService.IsPCOnAsync(_serverIp, ServerPort),
                    online = true
                }
            }
        };

        QueryResponseDto response = new QueryResponseDto
        {
            requestId = request.requestId,
            payload = new QueryResponsePayload
            {
                devices = devices
            }
        };

        return Ok(response);
    }

    private async Task<IActionResult> ExecuteCommand(ExecuteRequestDto request)
    {
        ExecuteResponseDto response = await ProcessExecuteRequestAsync(request);
        return Ok(response);
    }

    private void TurnOffMyPC()
    {
        _wakeOnLanService.SendUdpShutdownCommand(_serverIp, ServerShutdownPort);
        _wakeOnLanService.SendTcpCommand(
            _serverIp,
            ServerShutdownPort,
            WakeOnLanService.TcpCommand.Shutdown
        );
    }

    private void TurnOnMyPC()
    {
        _wakeOnLanService.SendMagicPacket(_deviceMac);
    }

    private async Task<ExecuteResponseDto> ProcessExecuteRequestAsync(ExecuteRequestDto request)
    {
        DeviceAttributes states = new DeviceAttributes
        {
            online = true,
            status = QueryStatus.SUCCESS,
            on = false
        };

        Execution execution = request.inputs[0].payload.commands[0].execution[0];

        switch (execution.command)
        {
            case DeviceCommands.OnOff:
                states.on = execution.Params.on;
                if (states.on)
                {
                    TurnOnMyPC();
                }
                else
                {
                    TurnOffMyPC();
                }

                break;
        }

        return new ExecuteResponseDto
        {
            requestId = request.requestId,
            payload = new ExecuteResponsePayload
            {
                commands = new[]
                {
                    new ExecuteResponseCommand
                    {
                        ids = new[] { PcDevice.id },
                        status = QueryStatus.SUCCESS,
                        states = states
                    }
                }
            }
        };
    }

    [HttpGet(PcControllerRoute.IsOnline)]
    public async Task<bool> IsPCOnAsync()
    {
        return await _wakeOnLanService.IsPCOnAsync(_serverIp, ServerPort);
    }
}