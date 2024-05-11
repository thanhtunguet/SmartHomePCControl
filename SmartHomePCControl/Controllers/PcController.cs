using Microsoft.AspNetCore.Mvc;
using System.Text.Json;
using SmartHomePCControl.Models;
using SmartHomePCControl.Services;
using Newtonsoft.Json;

namespace SmartHomePCControl.Controllers;

[ApiController]
[Route("/smart-home")]
public class GoogleHomeController : ControllerBase
{
    private const string DeviceId = "MyPC";

    private const string DeviceMac = "58:11:22:c8:57:67";

    private const string ServerIp = "192.168.97.3";

    private const int ServerPort = 3389;

    private const int ServerShutdownPort = 10675;

    private static readonly Device PcDevice = new()
    {
        id = DeviceId,
        type = "action.devices.types.SWITCH",
        traits = new[]
        {
            DeviceTraits.OnOff,
        },
        name = new DeviceName
        {
            name = "My PC",
        },
        willReportState = true,
        deviceInfo = new DeviceInfo
        {
            Manufacturer = "Asus",
            Model = "DesktopComputer",
            HwVersion = "Core i5 13400F",
            SwVersion = "Windows 11",
        }
    };

    [HttpPost]
    public IActionResult HandleSmartHomeRequest([FromBody] JsonElement request)
    {
        var jsonString = request.ToString();
        var requestDto = JsonConvert.DeserializeObject<RequestDto>(jsonString);
        if (requestDto != null)
        {
            string intent = requestDto.inputs[0].intent;

            Console.WriteLine($"Intent = {intent}");
            switch (intent)
            {
                case "action.devices.SYNC":
                    var syncRequestDto = JsonConvert.DeserializeObject<SyncRequestDto>(jsonString);
                    if (syncRequestDto != null)
                        return SyncDevices(syncRequestDto);
                    break;

                case "action.devices.QUERY":
                    var queryRequestDto = JsonConvert.DeserializeObject<QueryRequestDto>(jsonString);
                    if (queryRequestDto != null)
                        return QueryDevices(queryRequestDto);
                    break;

                case "action.devices.EXECUTE":
                    var executeRequestDto = JsonConvert.DeserializeObject<ExecuteRequestDto>(jsonString);
                    if (executeRequestDto != null)
                        return ExecuteCommand(executeRequestDto);
                    break;
            }
        }

        return BadRequest("Invalid intent");
    }

    private IActionResult SyncDevices(SyncRequestDto request)
    {
        // Create a sync response
        var response = new SyncResponseDto
        {
            requestId = request.requestId,
            payload = new SyncResponsePayload()
            {
                agentUserId = request.agentUserId,
                devices = new[]
                {
                    PcDevice,
                }
            }
        };

        // Return the response
        return Ok(response);
    }

    private IActionResult QueryDevices(QueryRequestDto request)
    {
        var devices = new Dictionary<string, DeviceAttributes>
        {
            {
                DeviceId,
                new DeviceAttributes()
                {
                    status = QueryStatus.SUCCESS,
                    on = WakeOnLan.IsPCOn(ServerIp, ServerPort),
                    online = true,
                }
            }
        };
        // Create a query response
        var response = new QueryResponseDto
        {
            requestId = request.requestId,
            payload = new QueryResponsePayload()
            {
                devices = devices,
            }
        };

        // Return the response
        return Ok(response);
    }


    private IActionResult ExecuteCommand(ExecuteRequestDto request)
    {
        // Process the execute request and perform the corresponding action
        ExecuteResponseDto response = ProcessExecuteRequest(request);

        // Return the response
        return Ok(response);
    }

    private ExecuteResponseDto ProcessExecuteRequest(ExecuteRequestDto request)
    {
        var states = new DeviceAttributes()
        {
            online = true,
            status = QueryStatus.SUCCESS,
            on = false,
        };

        var execution = request.inputs[0].payload.commands[0].execution[0];

        switch (execution.command)
        {
            case DeviceCommands.OnOff:
                states.on = execution.Params.on;
                if (states.on)
                {
                    // Turn on PC
                    WakeOnLan.SendMagicPacket(DeviceMac);
                    // Send Wakeup signal to Socket server
                    WakeOnLan.SendWakeUpSignal("192.168.97.2", 10675);
                }
                else
                {
                    // Turn off PC via UDP Socket
                    WakeOnLan.SendUdpShutdownCommand(ServerIp, ServerShutdownPort);
                    // Turn off PC via TCP Socket
                    WakeOnLan.SendTcpShutdownCommand(ServerIp, ServerShutdownPort);
                }

                break;

            case DeviceCommands.Reboot:
                states.on = execution.Params.on;
                WakeOnLan.SendRebootCommand("192.168.97.2", 10675);
                break;
        }

        return new ExecuteResponseDto
        {
            requestId = request.requestId,
            payload = new ExecuteResponsePayload
            {
                commands = new ExecuteResponseCommand[]
                {
                    new()
                    {
                        ids = new[]
                        {
                            DeviceId,
                        },
                        status = QueryStatus.SUCCESS,
                        states = states,
                    }
                }
            }
        };
    }
}