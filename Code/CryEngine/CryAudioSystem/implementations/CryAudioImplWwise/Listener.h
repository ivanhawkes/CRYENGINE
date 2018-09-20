// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ATLEntityData.h>
#include <AK/SoundEngine/Common/AkTypes.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CListener final : public IListener
{
public:

	CListener() = delete;
	CListener(CListener const&) = delete;
	CListener(CListener&&) = delete;
	CListener& operator=(CListener const&) = delete;
	CListener& operator=(CListener&&) = delete;

	explicit CListener(CObjectTransformation const& transformation, AkGameObjectID const id);

	virtual ~CListener() override = default;

	// CryAudio::Impl::IListener
	virtual void                         Update(float const deltaTime) override;
	virtual void                         SetName(char const* const szName) override;
	virtual void                         SetTransformation(CObjectTransformation const& transformation) override;
	virtual CObjectTransformation const& GetTransformation() const override { return m_transformation; }
	// ~CryAudio::Impl::IListener

	AkGameObjectID GetId() const       { return m_id; }
	bool           HasMoved() const    { return m_hasMoved; }
	Vec3 const&    GetPosition() const { return m_position; }
	Vec3 const&    GetVelocity() const { return m_velocity; }

#if defined(INCLUDE_WWISE_IMPL_PRODUCTION_CODE)
	char const* GetName() const { return m_name.c_str(); }
#endif  // INCLUDE_WWISE_IMPL_PRODUCTION_CODE

private:

	AkGameObjectID const  m_id;
	bool                  m_hasMoved;
	bool                  m_isMovingOrDecaying;
	Vec3                  m_velocity;
	Vec3                  m_position;
	Vec3                  m_previousPosition;
	CObjectTransformation m_transformation;

#if defined(INCLUDE_WWISE_IMPL_PRODUCTION_CODE)
	CryFixedStringT<MaxObjectNameLength> m_name;
#endif  // INCLUDE_WWISE_IMPL_PRODUCTION_CODE
};
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
