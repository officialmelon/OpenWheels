// OpenWheels: Box2D 2.0's sweep-and-prune broad-phase and pair manager (Box2DFlash 2.0.2
// b2BroadPhase, b2PairManager), the engine of the browser game. b2World keeps one while the
// browser game's physics runs (b2Flash20World.cpp): it decides when contacts are created and
// destroyed, in which order, and which bodies leave the world box and freeze.
//
// A proxy is created and destroyed at once (pairs are committed on the spot); a moved proxy
// buffers its pair changes until Commit. Proxy ids come back last in, first out. The world box and
// the boxes passed in are in the game's frame: with `mirrored` the 2.0 world is that one flipped
// at y = mirrorY (y_2.0 = mirrorY - y).

#ifndef B2_FLASH20_BROAD_PHASE_H
#define B2_FLASH20_BROAD_PHASE_H

#include "Box2D/Collision/b2Collision.h"

#include <unordered_map>
#include <vector>

class b2Flash20PairCallback
{
public:
	virtual ~b2Flash20PairCallback() {}

	/// A new pair of overlapping proxies (the one with the lower id first).
	virtual void Flash20PairAdded(void* userData1, void* userData2) = 0;

	/// A pair stopped overlapping, or one of its proxies is being destroyed.
	virtual void Flash20PairRemoved(void* userData1, void* userData2) = 0;
};

class b2Flash20BroadPhase
{
public:
	enum
	{
		e_nullProxy = 0xFFFF
	};

	/// worldLower / worldUpper: 2.0's world box, in 2.0's frame.
	b2Flash20BroadPhase(const b2Vec2& worldLower, const b2Vec2& worldUpper, bool mirrored,
		float32 mirrorY, b2Flash20PairCallback* callback);

	/// Whether a box is inside the world box (else 2.0 gives no proxy, or freezes the body).
	bool InRange(const b2AABB& aabb) const;

	int32 CreateProxy(const b2AABB& aabb, void* userData);
	void DestroyProxy(int32 proxyId);
	void MoveProxy(int32 proxyId, const b2AABB& aabb);
	void Commit();

	/// 2.0's b2BroadPhase::QueryAABB: the user data of up to maxCount proxies whose boxes overlap
	/// aabb, in 2.0's order (the order they meet the y axis bounds).
	int32 QueryAABB(const b2AABB& aabb, void** userData, int32 maxCount);

	int32 GetProxyCount() const { return m_proxyCount; }

private:
	enum
	{
		e_invalid = 0xFFFF,
		e_pairBuffered = 1,
		e_pairRemoved = 2,
		e_pairFinal = 4
	};

	struct Bound
	{
		bool IsLower() const { return (value & 1) == 0; }
		bool IsUpper() const { return (value & 1) == 1; }
		int32 value;
		int32 proxyId;
		int32 stabbingCount;
	};

	struct Proxy
	{
		int32 GetNext() const { return lowerBounds[0]; }
		void SetNext(int32 next) { lowerBounds[0] = next; }
		int32 lowerBounds[2];
		int32 upperBounds[2];
		int32 overlapCount;
		int32 timeStamp;
		void* userData;
	};

	struct BoundValues
	{
		int32 lowerValues[2];
		int32 upperValues[2];
	};

	struct Pair
	{
		int32 status;
	};

	struct BufferedPair
	{
		int32 proxyId1;
		int32 proxyId2;
	};

	static uint32 Key(int32 id1, int32 id2)
	{
		if (id1 > id2)
		{
			b2Swap(id1, id2);
		}
		return (uint32(id1) << 16) | uint32(id2);
	}

	void Grow();
	void ComputeBounds(int32* lowerValues, int32* upperValues, const b2AABB& aabb) const;
	bool TestOverlap(const BoundValues& b, const Proxy& p) const;
	void Query(int32* lowerIndex, int32* upperIndex, int32 lowerValue, int32 upperValue,
		const Bound* bounds, int32 boundCount, int32 axis);
	void IncrementOverlapCount(int32 proxyId);
	void IncrementTimeStamp();
	static int32 BinarySearch(const Bound* bounds, int32 count, int32 value);

	Pair* AddPair(int32 id1, int32 id2);
	Pair* FindPair(int32 id1, int32 id2);
	void AddBufferedPair(int32 id1, int32 id2);
	void RemoveBufferedPair(int32 id1, int32 id2);

	double m_worldLower[2];
	double m_worldUpper[2];
	double m_quantizationFactor[2];
	bool m_mirrored;
	double m_mirrorY;
	b2Flash20PairCallback* m_callback;

	std::vector<Proxy> m_proxyPool;
	std::vector<Bound> m_bounds[2];
	std::vector<int32> m_queryResults;
	int32 m_freeProxy;
	int32 m_proxyCount;
	int32 m_timeStamp;

	std::unordered_map<uint32, Pair> m_pairs;
	std::vector<BufferedPair> m_pairBuffer;
};

#endif
