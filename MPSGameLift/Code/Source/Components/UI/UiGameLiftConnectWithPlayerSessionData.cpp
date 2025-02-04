/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Components/UI/UiGameLiftConnectWithPlayerSessionData.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>

#include <LyShine/Bus/UiButtonBus.h>
#include <LyShine/Bus/UiCursorBus.h>
#include <LyShine/Bus/UiElementBus.h>
#include <LyShine/Bus/UiTextBus.h>
#include <LyShine/Bus/UiTextInputBus.h>
#include <LyShine/Bus/UiInteractableBus.h>

#include <Multiplayer/Session/SessionRequests.h>
#include <Request/AWSGameLiftSessionRequestBus.h>


namespace MPSGameLift
{
    void UiGameLiftConnectWithPlayerSessionData::Reflect(AZ::ReflectContext* context)
    {
        if (const auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<UiGameLiftConnectWithPlayerSessionData, AZ::Component>()
                ->Version(1)
                ->Field("ConnectButton", &UiGameLiftConnectWithPlayerSessionData::m_connectButtonUi)
                ->Field("ExitButton", &UiGameLiftConnectWithPlayerSessionData::m_quitButtonUi)
                ->Field("PlayerSessionDataInputUi", &UiGameLiftConnectWithPlayerSessionData::m_playerSessionDataJsonInputUi)
                ->Field("AttemptConnectionBlockerUi", &UiGameLiftConnectWithPlayerSessionData::m_attemptConnectionBlockerUi)
                ->Field("ConnectToHostFailedUi", &UiGameLiftConnectWithPlayerSessionData::m_connectToHostFailedUi)
                ->Field("JsonParseFailTextUi", &UiGameLiftConnectWithPlayerSessionData::m_jsonParseFailTextUi)
                ;

            if (AZ::EditContext* editContext = serializeContext->GetEditContext())
            {
                editContext->Class<UiGameLiftConnectWithPlayerSessionData>("UiGameLiftConnectWithPlayerSessionData", "Component to setup the start menu")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::Category, "Multiplayer Sample UI")
                    ->Attribute(AZ::Edit::Attributes::Icon, "Icons/Components/Component_Placeholder.svg")
                    ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("CanvasUI"))

                    ->DataElement(AZ::Edit::UIHandlers::Default, &UiGameLiftConnectWithPlayerSessionData::m_connectButtonUi, "Connect Button", "The UI button hosting a game (only available for unified launchers which can run as a client-host).")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UiGameLiftConnectWithPlayerSessionData::m_quitButtonUi, "Quit Button", "The UI button to quit the app.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UiGameLiftConnectWithPlayerSessionData::m_playerSessionDataJsonInputUi, "GameLift Player Session Text Input", "The UI text input providing the game session and player session id.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UiGameLiftConnectWithPlayerSessionData::m_attemptConnectionBlockerUi, "Attempt Connection Blocker", "Fullscreen UI for blocking user input while the client tries to connect.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UiGameLiftConnectWithPlayerSessionData::m_connectToHostFailedUi, "Connection To Host Failed", "UI to inform the user that connecting to the host failed.")
                    ->DataElement(AZ::Edit::UIHandlers::Default, &UiGameLiftConnectWithPlayerSessionData::m_jsonParseFailTextUi, "Json Parse Fail Text", "UI to inform the user that current JSON string is missing some expected data.")
                    ;
            }
        }
    }

    void UiGameLiftConnectWithPlayerSessionData::Activate()
    {
        Multiplayer::SessionAsyncRequestNotificationBus::Handler::BusConnect();
        UiCursorBus::Broadcast(&UiCursorInterface::IncrementVisibleCounter);

        // Listen for button presses
        UiButtonBus::Event(m_quitButtonUi, &UiButtonInterface::SetOnClickCallback, [this](AZ::EntityId buttonEntityId, [[maybe_unused]] AZ::Vector2 position) { OnButtonClicked(buttonEntityId); });
        UiButtonBus::Event(m_connectButtonUi, &UiButtonInterface::SetOnClickCallback, [this](AZ::EntityId buttonEntityId, [[maybe_unused]] AZ::Vector2 position) { OnButtonClicked(buttonEntityId); });
        UiButtonBus::Event(m_connectToHostFailedUi, &UiButtonInterface::SetOnClickCallback, [this](AZ::EntityId buttonEntityId, [[maybe_unused]] AZ::Vector2 position) { OnButtonClicked(buttonEntityId); });
        UiTextInputBus::Event(m_playerSessionDataJsonInputUi, &UiTextInputInterface::SetOnChangeCallback, [this]([[maybe_unused]] AZ::EntityId entityId, const AZStd::string& gameLiftJsonString) { OnJSONChanged(gameLiftJsonString); });

        // Hide the attempting connection ui until the player tries to connect
        UiElementBus::Event(m_attemptConnectionBlockerUi, &UiElementInterface::SetIsEnabled, false);

        // Listen for disconnect events to know if connecting to the host server failed
        AZ::Interface<Multiplayer::IMultiplayer>::Get()->AddEndpointDisconnectedHandler(m_onConnectToHostFailed);

        UiElementBus::Event(m_jsonParseFailTextUi, &UiElementInterface::SetIsEnabled, true);
        UiTextBus::Event(m_jsonParseFailTextUi, &UiTextInterface::SetText, "");
        UiInteractableBus::Event(m_connectButtonUi, &UiInteractableInterface::SetIsHandlingEvents, false);
        OnJSONChanged("");
    }

    void UiGameLiftConnectWithPlayerSessionData::Deactivate()
    {
        m_onConnectToHostFailed.Disconnect();
        UiCursorBus::Broadcast(&UiCursorInterface::DecrementVisibleCounter);

        Multiplayer::SessionAsyncRequestNotificationBus::Handler::BusDisconnect();
    }

    void UiGameLiftConnectWithPlayerSessionData::OnJSONChanged(const AZStd::string& gameLiftJsonString)
    {
        // Disable the connect button until checking to make sure the user has provided the proper GameLift information in JSON format
        UiInteractableBus::Event(m_connectButtonUi, &UiInteractableInterface::SetIsHandlingEvents, false);

        if (gameLiftJsonString.empty())
        {
            UiTextBus::Event(m_jsonParseFailTextUi, &UiTextInterface::SetText, "Please provide GameLift GameSessionId and PlayerSessionId in JSON format!");
            return;
        }
        
        // Parse GameLift JSON
        m_request = {};
        rapidjson::Document document;
        document.Parse(gameLiftJsonString.c_str());

        if (document.HasParseError())
        {
            UiTextBus::Event(m_jsonParseFailTextUi, &UiTextInterface::SetText, "Invalid JSON format!");
            return;
        }

        if (!document.HasMember("GameSessionId"))
        {
            UiTextBus::Event(m_jsonParseFailTextUi, &UiTextInterface::SetText, "Missing GameSessionId!");
            return;
        }

        if (!document.HasMember("PlayerSessionId"))
        {
            UiTextBus::Event(m_jsonParseFailTextUi, &UiTextInterface::SetText, "Missing PlayerSessionId!");
            return;
        }

        const rapidjson::Value& gameSessionId = document["GameSessionId"];
        if (!gameSessionId.IsString())
        {
            UiTextBus::Event(m_jsonParseFailTextUi, &UiTextInterface::SetText, "Invalid GameSessionId!");
            return;
        }

        const rapidjson::Value& playerSessionId = document["PlayerSessionId"];
        if (!playerSessionId.IsString())
        {
            UiTextBus::Event(m_jsonParseFailTextUi, &UiTextInterface::SetText, "Invalid PlayerSessionId!");
            return;
        }

        UiTextBus::Event(m_jsonParseFailTextUi, &UiTextInterface::SetText, "");
        m_request.m_sessionId = gameSessionId.GetString();
        m_request.m_playerId = playerSessionId.GetString();
        UiInteractableBus::Event(m_connectButtonUi, &UiInteractableInterface::SetIsHandlingEvents, true);
    }

    void UiGameLiftConnectWithPlayerSessionData::OnButtonClicked(AZ::EntityId buttonEntityId) const
    {
        const auto console = AZ::Interface<AZ::IConsole>::Get();
        if (!console)
        {
            AZ_Assert(false, "UiGameLiftConnectWithPlayerSessionData attempting to use console commands before AZ::Console is available.");
            return;
        }

        if (buttonEntityId == m_quitButtonUi)
        {
            console->PerformCommand("quit");
            return;
        }
        
        if (buttonEntityId == m_connectButtonUi)
        {
            // Enable blocker ui while we attempt connection
            UiElementBus::Event(m_attemptConnectionBlockerUi, &UiElementInterface::SetIsEnabled, true);

            AWSGameLift::AWSGameLiftSessionAsyncRequestBus::Broadcast(
                &AWSGameLift::AWSGameLiftSessionAsyncRequestBus::Events::JoinSessionAsync, m_request);
            return;
        }
        
        if (buttonEntityId == m_connectToHostFailedUi)
        {
            // Player acknowledged connection failed. Close the warning popup.
            UiElementBus::Event(m_connectToHostFailedUi, &UiElementInterface::SetIsEnabled, false);
        }
    }

    void UiGameLiftConnectWithPlayerSessionData::OnConnectToHostFailed()
    {
        UiElementBus::Event(m_attemptConnectionBlockerUi, &UiElementInterface::SetIsEnabled, false);
        UiElementBus::Event(m_connectToHostFailedUi, &UiElementInterface::SetIsEnabled, true);
    }

    void UiGameLiftConnectWithPlayerSessionData::OnJoinSessionAsyncComplete(bool joinSessionsResponse)
    {
        UiElementBus::Event(m_attemptConnectionBlockerUi, &UiElementInterface::SetIsEnabled, false);

        if (!joinSessionsResponse)
        {
            UiElementBus::Event(m_connectToHostFailedUi, &UiElementInterface::SetIsEnabled, true);
        }
    }

    
} // namespace MultiplayerSample
