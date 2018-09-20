// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "MNMTile.h"

struct INavMeshQueryFilter;
struct INavMeshQueryProcessing;

namespace MNM
{

//! Enum for results of NavMesh ray-cast queries
enum class ERayCastResult
{
	NoHit = 0,              //! Ray doesn't hit NavMesh boundary
	Hit,                    //! Ray hit NavMesh boundary and SRayHitOutput is filled
	RayTooLong,             //! Ray is too long and is going through too many triangles
	DisconnectedLocations,  //! Starting and ending positions can't be connected directly by straight line on NavMesh
	InvalidStart,           //! Starting position doesn't match with starting triangle
	InvalidEnd,             //! Ending position doesn't match with ending triangle
	Count
};

//! Structure used as output in NavMesh ray-cast queries
struct SRayHitOutput
{
	Vec3 position;
	Vec3 normal2D;
	float distance;
};

struct ITriangleColorSelector
{
	virtual ~ITriangleColorSelector() {}
	virtual ColorB GetAnnotationColor(AreaAnnotation annotation) const = 0;
};

struct ESnappingType
{
	enum ETypeVertical { Vertical };
	enum ETypeBox { Box };
	enum ETypeCircular { Circular };
};

//! Structure used in NavMesh queries for finding the nearest triangles using various methods of ranges
struct SSnappingMetric
{
	enum class EType
	{
		Vertical,   //! Nearest position on NavMesh either above or below the position
		Box,        //! Nearest position on NavMesh in axis aligned box 
		Circular,   //! Nearest position on NavMesh from triangles intersecting cylinder
		Count,
	};

	SSnappingMetric(ESnappingType::ETypeVertical, const float verticalUpRange = -FLT_MAX, const float verticalDownRange = -FLT_MAX)
		: SSnappingMetric(EType::Vertical, verticalUpRange, verticalDownRange)
	{}

	SSnappingMetric(ESnappingType::ETypeBox, const float verticalUpRange = -FLT_MAX, const float verticalDownRange = -FLT_MAX, const float horizontalRange = -FLT_MAX)
		: SSnappingMetric(EType::Box, verticalUpRange, verticalDownRange, horizontalRange)
	{}

	SSnappingMetric(ESnappingType::ETypeCircular, const float verticalUpRange = -FLT_MAX, const float verticalDownRange = -FLT_MAX, const float radius = -FLT_MAX)
		: SSnappingMetric(EType::Circular, verticalUpRange, verticalDownRange, radius)
	{}

private:
	SSnappingMetric(const EType type, const float verticalUpRange, const float verticalDownRange, const float horizontalRange = -FLT_MAX)
		: type(type)
		, verticalUpRange(verticalUpRange)
		, verticalDownRange(verticalDownRange)
		, horizontalRange(horizontalRange)
	{
		CRY_ASSERT_MESSAGE(verticalUpRange >= 0 || verticalUpRange == -FLT_MAX, "Snapping metric's vertical up range should have non-negative or default value!");
		CRY_ASSERT_MESSAGE(verticalDownRange >= 0 || verticalDownRange == -FLT_MAX, "Snapping metric's vertical down range should have non-negative or default value!");
		CRY_ASSERT_MESSAGE(verticalUpRange != verticalDownRange || verticalUpRange != 0.0f, "Snapping metric's vertical up and down ranges can't be both zero!");
		CRY_ASSERT_MESSAGE(type == EType::Vertical || horizontalRange != 0.0f, "Snapping metric's horizontal range can't be zero!");
	}

public:
	EType type;
	float verticalUpRange = -FLT_MAX;
	float verticalDownRange = -FLT_MAX;
	float horizontalRange = -FLT_MAX;
};

//! Wrapper structure for the ordered snapping metrics array. 'Ordered' means, that the metrics are checked in order from the first until any one succeeds.
struct SOrderedSnappingMetrics
{
	SOrderedSnappingMetrics() {}

	SOrderedSnappingMetrics(const SSnappingMetric& metric)
	{
		metricsArray.push_back(metric);
	}

	void AddMetric(const SSnappingMetric& metric)
	{
		metricsArray.push_back(metric);
	}

	template<class ... Vals>
	void EmplaceMetric(Vals&& ... vals)
	{
		metricsArray.emplace_back(std::forward<Vals>(vals) ...);
	}

	void CreateDefault()
	{
		metricsArray.emplace_back(ESnappingType::Vertical);
		metricsArray.emplace_back(ESnappingType::Box);
	}

	void Clear()
	{
		metricsArray.clear();
	}

	LocalDynArray<SSnappingMetric, 4> metricsArray;
};

namespace NavMesh
{

//! Mesh parameters.
struct SParams
{
	Vec3 originWorld;   //! Origin position of the NavMesh in the world coordinate space.
};

template<typename T>
inline T ToMeshSpace(const T& worldPosition, const T& meshOrigin)
{
	return worldPosition - meshOrigin;
}

template<typename T>
inline T ToWorldSpace(const T& localPosition, const T& meshOrigin)
{
	return localPosition + meshOrigin;
}

//! Returns the tile grid coordinate offset of the neighbor tile.
//! \param side Side of the tile on which neighbor tile is. Also, \see MNM::Tile::SLink::side.
//! \return Pointer to int[3] array with the (x,y,z) grid coordinate offset.
// #MNM_TODO pavloi 2016.08.15: represent side as an enum?
inline const int* GetNeighbourTileOffset(const size_t side)
{
	// #MNM_TODO pavloi 2016.08.15: is there a better solution than putting a static array into the inline function inside header file?
	static const int NeighbourOffset_MeshGrid[14][3] =
	{
		{ 1,  0,  0  },
		{ 1,  0,  1  },
		{ 1,  0,  -1 },

		{ 0,  1,  0  },
		{ 0,  1,  1  },
		{ 0,  1,  -1 },

		{ 0,  0,  1  },

		{ -1, 0,  0  },
		{ -1, 0,  -1 },
		{ -1, 0,  1  },

		{ 0,  -1, 0  },
		{ 0,  -1, -1 },
		{ 0,  -1, 1  },

		{ 0,  0,  -1 },
	};

	return NeighbourOffset_MeshGrid[side];
}

//! Returns the neighbor tile grid coordinate given the current tile gird coordinate and the neighbor side.
//! \param tileCoord Tile coordinate of the current tile.
//! \param side Side of the tile on which neighbor tile is. Also, \see MNM::Tile::SLink::side.
//! \return Tile grid coordinate of the neighbor tile.
// #MNM_TODO pavloi 2016.08.15: see comment about MNM::Tile::STileData::tileGridCoord about grid coord type
// #MNM_TODO pavloi 2016.08.15: produces negative coordinates - can be done better?
inline vector3_t GetNeighbourTileCoord(const vector3_t& tileCoord, const size_t side)
{
	const int* pOffset = GetNeighbourTileOffset(side);
	const vector3_t offset = vector3_t(real_t(pOffset[0]), real_t(pOffset[1]), real_t(pOffset[2]));
	return tileCoord + offset;
}

} // namespace NavMesh

//! Provides an access to the NavMesh data
struct INavMesh
{
	virtual ~INavMesh() {}

	//! Fills a NavMesh::SParams structure with a mesh parameters.
	virtual void GetMeshParams(NavMesh::SParams& outParams) const = 0;

	//! Finds a TileID by the tile's grid coordinate.
	//! \return Valid TileID, if such tile exists. Otherwise, return MNM::Constants::InvalidTileID.
	virtual TileID FindTileIDByTileGridCoord(const vector3_t& tileGridCoord) const = 0;

	//! Queries NavMesh triangles inside a bounding box.
	//! \param queryLocalAabb Bounding box for the query in a mesh coordinate space.
	//! \param pOptionalFilter Pointer to the optional triangle filter. If provided, then it's called for each triangleId.
	//! \param maxTrianglesCount Size of output buffer for result triangleId's.
	//! \param pOutTriangles Array buffer for result triangleId's.
	//! \return Count of found triangleId's.
	virtual size_t QueryTriangles(const aabb_t& queryLocalAabb, const INavMeshQueryFilter* pOptionalFilter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const = 0;

	//! Queries NavMesh triangles inside a bounding box and process them
	//! \param queryLocalAabb Bounding box for the query in a mesh coordinate space.
	//! \param pOptionalFilter Pointer to the optional triangle filter. If provided, then it's called for each triangleId.
	//! \param pQueryProcessing Pointer to the function object structure responsible for processing queried triangles. See INavMeshQueryProcessing.
	virtual void QueryTrianglesWithProcessing(const aabb_t& queryLocalAabb, const INavMeshQueryFilter* pOptionalFilter, INavMeshQueryProcessing* pQueryProcessing) const = 0;

	//! Finds a single triangle closest to the point. /see QueryTriangles() for a way to get candidate triangles.
	//! \param queryLocalPosition Query point in a mesh coordinate space.
	//! \param pCandidateTriangles Array of triangleID to search.
	//! \param candidateTrianglesCount Size of pCandidateTriangles array.
	//! \param pOutClosestLocalPosition Optional pointer for a return value of a snapped to the triangle point in the mesh coordinate space.
	//! \param pOutClosestDistanceSq Optional pointer for a return value of squared distance between query point and a snapped to the triangle point.
	//! \return ID of a found closest triangle.
	virtual TriangleID FindClosestTriangle(const vector3_t& queryLocalPosition, const TriangleID* pCandidateTriangles, const size_t candidateTrianglesCount, vector3_t* pOutClosestLocalPosition, float* pOutClosestDistanceSq) const = 0;

	//! Fills a STileData structure for a read-only tile data access
	//! \param tileId Id of tile
	//! \param outTileData STileData structure to fill
	//! \return true if tile is found and outTileData is filled.
	virtual bool GetTileData(const TileID tileId, Tile::STileData& outTileData) const = 0;

	//! Returns triangle area annotation
	//! \param triangleID id of the triangle
	//! \return Pointer to triangle area annotation or null if the triangle isn't found
	virtual const AreaAnnotation* GetTriangleAnnotation(TriangleID triangleID) const = 0;

	//! Returns whether the triangle can passes the NavMesh query filter
	//! \param triangleID Id of the triangle
	//! \param filter Query filter to check triangle with
	//! \return Returns true if the triangle is valid and passes the provided filter
	virtual bool CanTrianglePassFilter(const TriangleID triangleID, const INavMeshQueryFilter& filter) const = 0;
};

} // namespace MNM
