// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformBase.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Interface of a game platform service (Steam, PSN, Discord etc.)
		struct IService : public IBase
		{
			//! Listener to check for general game platform events
			//! See IService::AddListener and RemoveListener
			struct IListener
			{
				enum class EPersonaChangeFlags
				{
					Name = BIT(0),
					Status = BIT(1),
					CameOnline = BIT(2),
					WentOffline = BIT(3),
					GamePlayed = BIT(4),
					GameServer = BIT(5),
					ChangeAvatar = BIT(6),
					JoinedSource = BIT(7),
					LeftSource = BIT(8),
					RelationshipChanged = BIT(9),
					NameFirstSet = BIT(10),
					FacebookInfo = BIT(11),
					Nickname = BIT(12),
					SteamLevel = BIT(13),
				};

				virtual ~IListener() {}
				//! Called when the in-game platform layer is opened (usually by the user)
				virtual void OnOverlayActivated(const ServiceIdentifier& serviceId, bool active) = 0;
				//! Called when an avatar requested using RequestUserInformation has become available
				virtual void OnAvatarImageLoaded(const AccountIdentifier& accountId) = 0;
				//! Called when the service is about to shut down
				virtual void OnShutdown(const ServiceIdentifier& serviceId) = 0;
				//! Called when an account has been added
				virtual void OnAccountAdded(IAccount& account) = 0;
				//! Called right before removing an account
				virtual void OnAccountRemoved(IAccount& account) = 0;
				//! Called when the persona state was updated
				virtual void OnPersonaStateChanged(const IAccount& account, CEnumFlags<EPersonaChangeFlags> changeFlags) = 0;
				//! Called when a steam auth ticket request received a response
				virtual void OnGetSteamAuthTicketResponse(bool success, uint32 authTicket) = 0;
			};

			virtual ~IService() {}

			//! Adds a service event listener
			virtual void AddListener(IListener& listener) = 0;
			//! Removes a service event listener
			virtual void RemoveListener(IListener& listener) = 0;

			//! Called by core platform plugin before it's going to be unloaded
			virtual void Shutdown() = 0;

			//! Gets the unique identifier of this service
			virtual ServiceIdentifier GetServiceIdentifier() const = 0;

			// Returns the platform identifier of the build the player is running, usually the trivial version of the application version
			virtual int GetBuildIdentifier() const = 0;

			//! Checks if the local user owns the specified identifier
			//! \param id The platform-specific identifier for the application or DLC
			virtual bool OwnsApplication(ApplicationIdentifier id) const = 0;

			//! Gets the platform-specific identifier for the running application
			virtual ApplicationIdentifier GetApplicationIdentifier() const = 0;
			
			//! Gets an IAccount representation of the local user, useful for getting local information such as user name
			virtual IAccount* GetLocalAccount() const = 0;
			//! Gets local user's friend accounts
			virtual const DynArray<IAccount*>& GetFriendAccounts() const = 0;
			//! Gets an IAccount representation of another user by account id
			virtual IAccount* GetAccountById(const AccountIdentifier& accountId) const = 0;

			//! Checks if the local user has the other user in their friends list for this service
			virtual bool IsFriendWith(const AccountIdentifier& otherAccountId) const = 0;
			//! Gets the relationship status between the local user and another user
			virtual EFriendRelationship GetFriendRelationship(const AccountIdentifier& otherAccountId) const = 0;
			//! Opens a known dialog targeted at a specific user id via the platform's overlay
			virtual bool OpenDialogWithTargetUser(EUserTargetedDialog dialog, const AccountIdentifier& otherAccountId) const = 0;
			//! Opens a known dialog by platform-specific string and targeted at a specific user id via the platform's overlay
			virtual bool OpenDialogWithTargetUser(const char* szPage, const AccountIdentifier& otherAccountId) const = 0;

			//! Opens a known dialog via the platforms's overlay
			virtual bool OpenDialog(EDialog dialog) const = 0;
			//! Opens a known dialog by platform-specific string via the platform's overlay
			virtual bool OpenDialog(const char* szPage) const = 0;
			//! Opens a browser window via the platform's overlay
			virtual bool OpenBrowser(const char* szURL) const = 0;
			//! Checks whether we are able to open the overlay used for purchasing assets
			virtual bool CanOpenPurchaseOverlay() const = 0;

			//! Check if information about a user (e.g. personal name, avatar...) is available, otherwise download it.
			//! \note It is recommended to limit requests for bulky data (like avatars) to a minimum as some platforms may have bandwitdth or other limitations.
			//! \retval true Information is not yet available and listeners will be notified once retrieved.
			//! \retval false Information is already available and there's no need for a download.
			virtual bool RequestUserInformation(const AccountIdentifier& accountId, UserInformationMask info) = 0;

			//! CHeck if there is an active connection to the service's backend
			virtual bool IsLoggedIn() const = 0;
		};
	}
}