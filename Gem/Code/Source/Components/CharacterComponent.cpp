/*
 * All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
 * its licensors.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this
 * distribution (the "License"). All use of this software is governed by the License,
 * or, if provided, by the license below or the license accompanying this file. Do not
 * remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 */

#include <Source/Components/CharacterComponent.h>
#include <Multiplayer/Components/NetworkTransformComponent.h>

namespace MultiplayerSample
{
    void CharacterComponent::CharacterComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<CharacterComponent, CharacterComponentBase>()
                ->Version(1);
        }
        CharacterComponentBase::Reflect(context);
    }

    void CharacterComponent::OnInit()
    {
    }

    void CharacterComponent::OnActivate([[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
    }

    void CharacterComponent::OnDeactivate([[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
    }

    CharacterComponentController::CharacterComponentController(CharacterComponent& parent)
        : CharacterComponentControllerBase(parent)
    {
    }

    void CharacterComponentController::OnActivate([[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
    }

    void CharacterComponentController::OnDeactivate([[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
    }

    AZ::Vector3 CharacterComponentController::TryMoveToPosition([[maybe_unused]] const AZ::Transform& targetTransform, [[maybe_unused]] float deltaTime)
    {
        return GetNetworkTransformComponentController()->GetTranslation();
    }
}
