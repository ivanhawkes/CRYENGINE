// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IGamePlatform.h"
#include "IPlatformService.h"
#include "User.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Game platform core plugin. Keeps track and coordinates all available services.
		class CPlugin final : public IPlugin, public IService::IListener
		{
			CRYINTERFACE_BEGIN()
				CRYINTERFACE_ADD(Cry::GamePlatform::IPlugin)
				CRYINTERFACE_ADD(Cry::IEnginePlugin)
			CRYINTERFACE_END()

			CRYGENERATE_SINGLETONCLASS_GUID(CPlugin, "Plugin_GamePlatform", "{D1579580-15AB-458C-81AB-05068B275483}"_cry_guid)

		public:
			CPlugin();
			virtual ~CPlugin();

			// Cry::IEnginePlugin
			virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
			virtual void MainUpdate(float frameTime) override;
			virtual const char* GetName() const override { return "GamePlatform"; }
			virtual const char* GetCategory() const override { return "GamePlatform"; }
			// ~Cry::IEnginePlugin

			// IPlugin
			virtual IUser* GetLocalClient() const override;
			virtual IUser* GetUserById(const UserIdentifier& id) const override;
			virtual IUser* GetUserById(const AccountIdentifier& accountId) const override;

			virtual const DynArray<IUser*>& GetFriends() const override;

			virtual IServer* CreateServer(bool bLocal) override;
			virtual IServer* GetLocalServer() const override;

			virtual ILeaderboards* GetLeaderboards() const override;
			virtual IStatistics* GetStatistics() const override;
			virtual IRemoteStorage* GetRemoteStorage() const override;

			virtual IMatchmaking* GetMatchmaking() const override;

			virtual INetworking* GetNetworking() const override;

			virtual bool GetAuthToken(string &tokenOut, int &issuerId) override;

			virtual EConnectionStatus GetConnectionStatus() const override;
			virtual void CanAccessMultiplayerServices(std::function<void(bool authorized)> asynchronousCallback) override { asynchronousCallback(true); }

			virtual IService* GetMainService() const override;
			virtual IService* GetService(const ServiceIdentifier& svcId) const override;
			// ~IPlugin

			// IService::IListener
			virtual void OnOverlayActivated(const ServiceIdentifier& serviceId, bool active) override {}
			virtual void OnAvatarImageLoaded(const AccountIdentifier& accountId) override {}
			virtual void OnShutdown(const ServiceIdentifier& serviceId) override;
			virtual void OnAccountAdded(IAccount& account) override;
			virtual void OnAccountRemoved(IAccount& account) override;
			// ~IService::IListener

			virtual void RegisterMainService(IService& service) override;
			virtual void RegisterService(IService& service) override;

		private:
			ServiceIdentifier GetMainServiceIdentifier() const;
			void UnregisterService(const ServiceIdentifier& service);

			IUser* TryGetUser(const UserIdentifier& id) const;
			IUser* TryGetUser(const AccountIdentifier& id) const;
			IUser* TryGetUser(IAccount& account) const;
			IUser* AddUser(IAccount& account) const;

			void AddOrUpdateUser(DynArray<IAccount*> userAccounts);

			IAccount* GetAccount(const AccountIdentifier& id) const;
			IAccount* GetMainLocalAccount() const;

			void CollectConnectedAccounts(IAccount &account, DynArray<IAccount *>& userAccounts) const;
			void AddAccountConnections(const IAccount& account, DynArray<IAccount*>& userAccounts) const;
			DynArray<IAccount*>::iterator FindMainAccount(DynArray<IAccount*>& userAccounts) const;
			bool EnsureMainAccountFirst(DynArray<IAccount*>& userAccounts) const;

		private:
			// Index '0' is reserved for main service
			DynArray<IService*> m_services;

			mutable std::vector<std::unique_ptr<CUser>> m_users;
			mutable DynArray<IUser*> m_friends;
		};
	}
}