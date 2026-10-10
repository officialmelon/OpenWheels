// OpenWheels: Box2D 2.0's sweep-and-prune broad-phase (b2Flash20BroadPhase.h), ported from
// Box2DFlash 2.0.2 as the browser game runs it (b2_maxProxies 1024, 16-bit quantized bounds).

#include "Box2D/Collision/b2Flash20BroadPhase.h"

#include <cstring>

namespace
{
const int32 kInitialProxies = 1024;
const int32 kMaxValue = 0xFFFF;
}

b2Flash20BroadPhase::b2Flash20BroadPhase(const b2Vec2& worldLower, const b2Vec2& worldUpper,
	bool mirrored, float32 mirrorY, b2Flash20PairCallback* callback)
{
	m_worldLower[0] = worldLower.x;
	m_worldLower[1] = worldLower.y;
	m_worldUpper[0] = worldUpper.x;
	m_worldUpper[1] = worldUpper.y;
	m_quantizationFactor[0] = double(kMaxValue) / (m_worldUpper[0] - m_worldLower[0]);
	m_quantizationFactor[1] = double(kMaxValue) / (m_worldUpper[1] - m_worldLower[1]);
	m_mirrored = mirrored;
	m_mirrorY = mirrorY;
	m_callback = callback;
	m_freeProxy = e_nullProxy;
	m_proxyCount = 0;
	m_timeStamp = 1;
	Grow();
}

// The pool's free list runs 0, 1, 2, ... like 2.0's; past its 1024 proxies (where 2.0 would fail)
// it grows.
void b2Flash20BroadPhase::Grow()
{
	const int32 oldCount = int32(m_proxyPool.size());
	const int32 newCount = oldCount == 0 ? kInitialProxies : 2 * oldCount;
	b2Assert(newCount < e_nullProxy);
	m_proxyPool.resize(newCount);
	for (int32 i = oldCount; i < newCount; ++i)
	{
		Proxy& proxy = m_proxyPool[i];
		proxy.SetNext(i + 1 < newCount ? i + 1 : int32(m_freeProxy));
		proxy.lowerBounds[1] = proxy.upperBounds[0] = proxy.upperBounds[1] = e_invalid;
		proxy.timeStamp = 0;
		proxy.overlapCount = e_invalid;
		proxy.userData = nullptr;
	}
	m_freeProxy = oldCount;
	m_bounds[0].resize(2 * newCount);
	m_bounds[1].resize(2 * newCount);
	m_queryResults.reserve(newCount);
}

bool b2Flash20BroadPhase::InRange(const b2AABB& aabb) const
{
	double lowerY = aabb.lowerBound.y, upperY = aabb.upperBound.y;
	if (m_mirrored)
	{
		lowerY = m_mirrorY - aabb.upperBound.y;
		upperY = m_mirrorY - aabb.lowerBound.y;
	}
	const double dx = b2Max(double(aabb.lowerBound.x) - m_worldUpper[0], m_worldLower[0] - aabb.upperBound.x);
	const double dy = b2Max(lowerY - m_worldUpper[1], m_worldLower[1] - upperY);
	return b2Max(dx, dy) < 0.0;
}

void b2Flash20BroadPhase::ComputeBounds(int32* lowerValues, int32* upperValues, const b2AABB& aabb) const
{
	double lower[2] = {aabb.lowerBound.x, aabb.lowerBound.y};
	double upper[2] = {aabb.upperBound.x, aabb.upperBound.y};
	if (m_mirrored)
	{
		lower[1] = m_mirrorY - aabb.upperBound.y;
		upper[1] = m_mirrorY - aabb.lowerBound.y;
	}
	for (int32 axis = 0; axis < 2; ++axis)
	{
		const double l = b2Max(b2Min(lower[axis], m_worldUpper[axis]), m_worldLower[axis]);
		const double u = b2Max(b2Min(upper[axis], m_worldUpper[axis]), m_worldLower[axis]);
		lowerValues[axis] = int32(m_quantizationFactor[axis] * (l - m_worldLower[axis])) & (kMaxValue - 1);
		upperValues[axis] = (int32(m_quantizationFactor[axis] * (u - m_worldLower[axis])) & kMaxValue) | 1;
	}
}

bool b2Flash20BroadPhase::TestOverlap(const BoundValues& b, const Proxy& p) const
{
	for (int32 axis = 0; axis < 2; ++axis)
	{
		const std::vector<Bound>& bounds = m_bounds[axis];
		if (b.lowerValues[axis] > bounds[p.upperBounds[axis]].value)
		{
			return false;
		}
		if (b.upperValues[axis] < bounds[p.lowerBounds[axis]].value)
		{
			return false;
		}
	}
	return true;
}

int32 b2Flash20BroadPhase::BinarySearch(const Bound* bounds, int32 count, int32 value)
{
	int32 low = 0;
	int32 high = count - 1;
	while (low <= high)
	{
		const int32 mid = (low + high) / 2;
		if (bounds[mid].value > value)
		{
			high = mid - 1;
		}
		else if (bounds[mid].value < value)
		{
			low = mid + 1;
		}
		else
		{
			return mid;
		}
	}
	return low;
}

void b2Flash20BroadPhase::Query(int32* lowerQueryOut, int32* upperQueryOut, int32 lowerValue,
	int32 upperValue, const Bound* bounds, int32 boundCount, int32 axis)
{
	const int32 lowerQuery = BinarySearch(bounds, boundCount, lowerValue);
	const int32 upperQuery = BinarySearch(bounds, boundCount, upperValue);

	// Easy case: lowerQuery <= lowerIndex(i) < upperQuery
	for (int32 i = lowerQuery; i < upperQuery; ++i)
	{
		if (bounds[i].IsLower())
		{
			IncrementOverlapCount(bounds[i].proxyId);
		}
	}

	// Hard case: lowerIndex(i) < lowerQuery < upperIndex(i)
	if (lowerQuery > 0)
	{
		int32 i = lowerQuery - 1;
		int32 s = bounds[i].stabbingCount;
		while (s)
		{
			if (bounds[i].IsLower())
			{
				const Proxy& proxy = m_proxyPool[bounds[i].proxyId];
				if (lowerQuery <= proxy.upperBounds[axis])
				{
					IncrementOverlapCount(bounds[i].proxyId);
					--s;
				}
			}
			--i;
		}
	}

	*lowerQueryOut = lowerQuery;
	*upperQueryOut = upperQuery;
}

void b2Flash20BroadPhase::IncrementOverlapCount(int32 proxyId)
{
	Proxy& proxy = m_proxyPool[proxyId];
	if (proxy.timeStamp < m_timeStamp)
	{
		proxy.timeStamp = m_timeStamp;
		proxy.overlapCount = 1;
	}
	else
	{
		proxy.overlapCount = 2;
		m_queryResults.push_back(proxyId);
	}
}

void b2Flash20BroadPhase::IncrementTimeStamp()
{
	if (m_timeStamp == kMaxValue)
	{
		for (Proxy& proxy : m_proxyPool)
		{
			proxy.timeStamp = 0;
		}
		m_timeStamp = 1;
	}
	else
	{
		++m_timeStamp;
	}
}

int32 b2Flash20BroadPhase::CreateProxy(const b2AABB& aabb, void* userData)
{
	if (m_freeProxy == e_nullProxy)
	{
		Grow();
	}

	const int32 proxyId = m_freeProxy;
	Proxy* proxy = &m_proxyPool[proxyId];
	m_freeProxy = proxy->GetNext();
	proxy->overlapCount = 0;
	proxy->userData = userData;

	const int32 boundCount = 2 * m_proxyCount;

	int32 lowerValues[2], upperValues[2];
	ComputeBounds(lowerValues, upperValues, aabb);

	for (int32 axis = 0; axis < 2; ++axis)
	{
		Bound* bounds = m_bounds[axis].data();
		int32 lowerIndex, upperIndex;
		Query(&lowerIndex, &upperIndex, lowerValues[axis], upperValues[axis], bounds, boundCount, axis);

		memmove(bounds + upperIndex + 2, bounds + upperIndex, (boundCount - upperIndex) * sizeof(Bound));
		memmove(bounds + lowerIndex + 1, bounds + lowerIndex, (upperIndex - lowerIndex) * sizeof(Bound));

		// The upper index has increased because of the lower bound insertion.
		++upperIndex;

		// Copy in the new bounds.
		bounds[lowerIndex].value = lowerValues[axis];
		bounds[lowerIndex].proxyId = proxyId;
		bounds[upperIndex].value = upperValues[axis];
		bounds[upperIndex].proxyId = proxyId;

		bounds[lowerIndex].stabbingCount = lowerIndex == 0 ? 0 : bounds[lowerIndex - 1].stabbingCount;
		bounds[upperIndex].stabbingCount = bounds[upperIndex - 1].stabbingCount;

		// Adjust the stabbing count between the new bounds.
		for (int32 index = lowerIndex; index < upperIndex; ++index)
		{
			++bounds[index].stabbingCount;
		}

		// Adjust the all the affected bound indices.
		for (int32 index = lowerIndex; index < boundCount + 2; ++index)
		{
			Proxy& p = m_proxyPool[bounds[index].proxyId];
			if (bounds[index].IsLower())
			{
				p.lowerBounds[axis] = index;
			}
			else
			{
				p.upperBounds[axis] = index;
			}
		}
	}

	++m_proxyCount;

	for (int32 id : m_queryResults)
	{
		AddBufferedPair(proxyId, id);
	}
	Commit();

	// Prepare for next query.
	m_queryResults.clear();
	IncrementTimeStamp();

	return proxyId;
}

int32 b2Flash20BroadPhase::QueryAABB(const b2AABB& aabb, void** userData, int32 maxCount)
{
	int32 lowerValues[2], upperValues[2];
	ComputeBounds(lowerValues, upperValues, aabb);

	int32 lowerIndex, upperIndex;
	const int32 boundCount = 2 * m_proxyCount;
	Query(&lowerIndex, &upperIndex, lowerValues[0], upperValues[0], m_bounds[0].data(), boundCount, 0);
	Query(&lowerIndex, &upperIndex, lowerValues[1], upperValues[1], m_bounds[1].data(), boundCount, 1);

	int32 count = 0;
	for (int32 i = 0; i < int32(m_queryResults.size()) && count < maxCount; ++i, ++count)
	{
		userData[i] = m_proxyPool[m_queryResults[i]].userData;
	}

	// Prepare for next query.
	m_queryResults.clear();
	IncrementTimeStamp();
	return count;
}

void b2Flash20BroadPhase::DestroyProxy(int32 proxyId)
{
	Proxy* proxy = &m_proxyPool[proxyId];
	const int32 boundCount = 2 * m_proxyCount;

	for (int32 axis = 0; axis < 2; ++axis)
	{
		Bound* bounds = m_bounds[axis].data();

		const int32 lowerIndex = proxy->lowerBounds[axis];
		const int32 upperIndex = proxy->upperBounds[axis];
		const int32 lowerValue = bounds[lowerIndex].value;
		const int32 upperValue = bounds[upperIndex].value;

		memmove(bounds + lowerIndex, bounds + lowerIndex + 1, (upperIndex - lowerIndex - 1) * sizeof(Bound));
		memmove(bounds + upperIndex - 1, bounds + upperIndex + 1, (boundCount - upperIndex - 1) * sizeof(Bound));

		// Fix bound indices.
		for (int32 index = lowerIndex; index < boundCount - 2; ++index)
		{
			Proxy& p = m_proxyPool[bounds[index].proxyId];
			if (bounds[index].IsLower())
			{
				p.lowerBounds[axis] = index;
			}
			else
			{
				p.upperBounds[axis] = index;
			}
		}

		// Fix stabbing count.
		for (int32 index = lowerIndex; index < upperIndex - 1; ++index)
		{
			--bounds[index].stabbingCount;
		}

		// Query for pairs to be removed. lowerIndex and upperIndex are not needed.
		int32 ignored0, ignored1;
		Query(&ignored0, &ignored1, lowerValue, upperValue, bounds, boundCount - 2, axis);
	}

	for (int32 id : m_queryResults)
	{
		RemoveBufferedPair(proxyId, id);
	}
	Commit();

	// Prepare for next query.
	m_queryResults.clear();
	IncrementTimeStamp();

	// Return the proxy to the pool.
	proxy = &m_proxyPool[proxyId];
	proxy->userData = nullptr;
	proxy->overlapCount = e_invalid;
	proxy->lowerBounds[0] = proxy->lowerBounds[1] = e_invalid;
	proxy->upperBounds[0] = proxy->upperBounds[1] = e_invalid;
	proxy->SetNext(m_freeProxy);
	m_freeProxy = proxyId;
	--m_proxyCount;
}

void b2Flash20BroadPhase::MoveProxy(int32 proxyId, const b2AABB& aabb)
{
	if (proxyId == e_nullProxy || proxyId >= int32(m_proxyPool.size()) || !aabb.IsValid())
	{
		return;
	}

	const int32 boundCount = 2 * m_proxyCount;
	Proxy* proxy = &m_proxyPool[proxyId];

	// Get new bound values
	BoundValues newValues;
	ComputeBounds(newValues.lowerValues, newValues.upperValues, aabb);

	// Get old bound values
	BoundValues oldValues;
	for (int32 axis = 0; axis < 2; ++axis)
	{
		oldValues.lowerValues[axis] = m_bounds[axis][proxy->lowerBounds[axis]].value;
		oldValues.upperValues[axis] = m_bounds[axis][proxy->upperBounds[axis]].value;
	}

	for (int32 axis = 0; axis < 2; ++axis)
	{
		Bound* bounds = m_bounds[axis].data();

		const int32 lowerIndex = proxy->lowerBounds[axis];
		const int32 upperIndex = proxy->upperBounds[axis];

		const int32 lowerValue = newValues.lowerValues[axis];
		const int32 upperValue = newValues.upperValues[axis];

		const int32 deltaLower = lowerValue - bounds[lowerIndex].value;
		const int32 deltaUpper = upperValue - bounds[upperIndex].value;

		bounds[lowerIndex].value = lowerValue;
		bounds[upperIndex].value = upperValue;

		//
		// Expanding adds overlaps
		//

		// Should we move the lower bound down?
		if (deltaLower < 0)
		{
			int32 index = lowerIndex;
			while (index > 0 && lowerValue < bounds[index - 1].value)
			{
				Bound* bound = bounds + index;
				Bound* prevBound = bound - 1;

				const int32 prevProxyId = prevBound->proxyId;
				Proxy* prevProxy = &m_proxyPool[prevBound->proxyId];

				++prevBound->stabbingCount;

				if (prevBound->IsUpper())
				{
					if (TestOverlap(newValues, *prevProxy))
					{
						AddBufferedPair(proxyId, prevProxyId);
					}

					++prevProxy->upperBounds[axis];
					++bound->stabbingCount;
				}
				else
				{
					++prevProxy->lowerBounds[axis];
					--bound->stabbingCount;
				}

				--proxy->lowerBounds[axis];
				b2Swap(*bound, *prevBound);
				--index;
			}
		}

		// Should we move the upper bound up?
		if (deltaUpper > 0)
		{
			int32 index = upperIndex;
			while (index < boundCount - 1 && bounds[index + 1].value <= upperValue)
			{
				Bound* bound = bounds + index;
				Bound* nextBound = bound + 1;
				const int32 nextProxyId = nextBound->proxyId;
				Proxy* nextProxy = &m_proxyPool[nextProxyId];

				++nextBound->stabbingCount;

				if (nextBound->IsLower())
				{
					if (TestOverlap(newValues, *nextProxy))
					{
						AddBufferedPair(proxyId, nextProxyId);
					}

					--nextProxy->lowerBounds[axis];
					++bound->stabbingCount;
				}
				else
				{
					--nextProxy->upperBounds[axis];
					--bound->stabbingCount;
				}

				++proxy->upperBounds[axis];
				b2Swap(*bound, *nextBound);
				++index;
			}
		}

		//
		// Shrinking removes overlaps
		//

		// Should we move the lower bound up?
		if (deltaLower > 0)
		{
			int32 index = lowerIndex;
			while (index < boundCount - 1 && bounds[index + 1].value <= lowerValue)
			{
				Bound* bound = bounds + index;
				Bound* nextBound = bound + 1;

				const int32 nextProxyId = nextBound->proxyId;
				Proxy* nextProxy = &m_proxyPool[nextProxyId];

				--nextBound->stabbingCount;

				if (nextBound->IsUpper())
				{
					if (TestOverlap(oldValues, *nextProxy))
					{
						RemoveBufferedPair(proxyId, nextProxyId);
					}

					--nextProxy->upperBounds[axis];
					--bound->stabbingCount;
				}
				else
				{
					--nextProxy->lowerBounds[axis];
					++bound->stabbingCount;
				}

				++proxy->lowerBounds[axis];
				b2Swap(*bound, *nextBound);
				++index;
			}
		}

		// Should we move the upper bound down?
		if (deltaUpper < 0)
		{
			int32 index = upperIndex;
			while (index > 0 && upperValue < bounds[index - 1].value)
			{
				Bound* bound = bounds + index;
				Bound* prevBound = bound - 1;

				const int32 prevProxyId = prevBound->proxyId;
				Proxy* prevProxy = &m_proxyPool[prevProxyId];

				--prevBound->stabbingCount;

				if (prevBound->IsLower())
				{
					if (TestOverlap(oldValues, *prevProxy))
					{
						RemoveBufferedPair(proxyId, prevProxyId);
					}

					++prevProxy->lowerBounds[axis];
					--bound->stabbingCount;
				}
				else
				{
					++prevProxy->upperBounds[axis];
					++bound->stabbingCount;
				}

				--proxy->upperBounds[axis];
				b2Swap(*bound, *prevBound);
				--index;
			}
		}
	}
}

b2Flash20BroadPhase::Pair* b2Flash20BroadPhase::FindPair(int32 id1, int32 id2)
{
	auto it = m_pairs.find(Key(id1, id2));
	return it == m_pairs.end() ? nullptr : &it->second;
}

b2Flash20BroadPhase::Pair* b2Flash20BroadPhase::AddPair(int32 id1, int32 id2)
{
	Pair& pair = m_pairs[Key(id1, id2)];
	return &pair;
}

void b2Flash20BroadPhase::AddBufferedPair(int32 id1, int32 id2)
{
	const bool found = m_pairs.count(Key(id1, id2)) != 0;
	Pair* pair = AddPair(id1, id2);
	if (!found)
	{
		pair->status = 0;
	}

	// If this pair is not in the pair buffer ...
	if ((pair->status & e_pairBuffered) == 0)
	{
		// ... then add it to the pair buffer.
		pair->status |= e_pairBuffered;
		BufferedPair buffered;
		buffered.proxyId1 = b2Min(id1, id2);
		buffered.proxyId2 = b2Max(id1, id2);
		m_pairBuffer.push_back(buffered);
	}

	// Confirm this pair for the subsequent call to Commit.
	pair->status &= ~e_pairRemoved;
}

void b2Flash20BroadPhase::RemoveBufferedPair(int32 id1, int32 id2)
{
	Pair* pair = FindPair(id1, id2);
	if (pair == nullptr)
	{
		// The pair never existed. This is legal (due to collision filtering).
		return;
	}

	// If this pair is not in the pair buffer ...
	if ((pair->status & e_pairBuffered) == 0)
	{
		// ... then add it to the pair buffer.
		pair->status |= e_pairBuffered;
		BufferedPair buffered;
		buffered.proxyId1 = b2Min(id1, id2);
		buffered.proxyId2 = b2Max(id1, id2);
		m_pairBuffer.push_back(buffered);
	}

	pair->status |= e_pairRemoved;
}

void b2Flash20BroadPhase::Commit()
{
	int32 removeCount = 0;

	for (int32 i = 0; i < int32(m_pairBuffer.size()); ++i)
	{
		const BufferedPair buffered = m_pairBuffer[i];
		Pair* pair = FindPair(buffered.proxyId1, buffered.proxyId2);
		b2Assert(pair != nullptr && (pair->status & e_pairBuffered));
		pair->status &= ~e_pairBuffered;

		void* userData1 = m_proxyPool[buffered.proxyId1].userData;
		void* userData2 = m_proxyPool[buffered.proxyId2].userData;

		if (pair->status & e_pairRemoved)
		{
			// It is possible a pair was added then removed before a commit. Therefore,
			// we should be careful not to tell the user the pair was removed when the
			// the user didn't receive a matching add.
			if (pair->status & e_pairFinal)
			{
				m_callback->Flash20PairRemoved(userData1, userData2);
			}

			// Store the ids so we can actually remove the pair below.
			m_pairBuffer[removeCount++] = buffered;
		}
		else if ((pair->status & e_pairFinal) == 0)
		{
			m_callback->Flash20PairAdded(userData1, userData2);
			pair->status |= e_pairFinal;
		}
	}

	for (int32 i = 0; i < removeCount; ++i)
	{
		m_pairs.erase(Key(m_pairBuffer[i].proxyId1, m_pairBuffer[i].proxyId2));
	}

	m_pairBuffer.clear();
}
