#include "w3_Window.h"
#include <memory>

WindowGrid::WindowGrid(LPRECT pMonitorRect, float dpiScaling) :
	m_DefaultWidthWeight(1), m_MonitorRect(*pMonitorRect),
	m_DpiScaling(dpiScaling)
{
}

bool WindowGrid::Insert(HWND hwnd)
{
	// Make sure window isn't min/max-imized
	ShowWindow(hwnd, SW_RESTORE);

	// Make new right-most column for window
	if(!InsertColumn(m_Grid.ColumnCount()))
	{
		return false;
	}

	// Place window in column
	m_Grid.InsertElement(m_Grid.ColumnCount()-1, 0, {hwnd, 1});
	m_Grid[m_Grid.ColumnCount()-1].m_TotalHeightWeight = 1;

	// Set focus to inserted window
	SetFocus(hwnd);

	return true;
}

bool WindowGrid::InsertColumn(size_t colPos)
{
	// Update total width weight
	m_TotalWidthWeight += m_DefaultWidthWeight;

	// Ensure total width weight hasn't overflowed
	if(m_TotalWidthWeight < m_DefaultWidthWeight)
	{
		m_TotalWidthWeight -= m_DefaultWidthWeight;
		return false;
	}

	m_Grid.InsertColumn(colPos, m_DefaultWidthWeight);

	return true;
}

void WindowGrid::RemoveColumn(size_t colPos)
{
	assert(colPos < m_Grid.ColumnCount());

	// Update total width weight
	m_TotalWidthWeight -= m_Grid[colPos].m_WidthWeight;

	m_Grid.RemoveColumn(colPos);
}

void WindowGrid::Apply()
{
	LONG totalWidth = m_MonitorRect.right - m_MonitorRect.left;
	LONG totalHeight = m_MonitorRect.bottom - m_MonitorRect.top;
	float widthUnit = float(totalWidth) / m_TotalWidthWeight;

	auto colCount = m_Grid.ColumnCount();
	decltype(colCount) i;
	LONG currentRight, currentLeft = m_MonitorRect.left;
	for(i = 0; i < colCount-1; ++i)
	{
		// Calculate left/right bounds of windows in this column
		currentRight = currentLeft + LONG(m_Grid[i].m_WidthWeight*widthUnit);

		// Set pos & size of windows in column
	colHandle:
		DEBUG_MESSAGE(_T("Column Dimensions"), _T("Window %d\nleft: %d\nright: %d"),
			m_Grid[i][0].m_Hwnd, currentLeft, currentRight);

		LONG currentBottom, currentTop = m_MonitorRect.top;
		float heightUnit = float(totalHeight) / m_Grid[i].m_TotalHeightWeight;

		// TODO: Need to handle bottom edge-case for rows
		for(auto &node : m_Grid[i])
		{
			// Calculate height of current window
			currentBottom = currentTop + LONG(node.m_HeightWeight*heightUnit);

			// Set pos & size of current window
			float scaling = true ? m_DpiScaling : 1.f; // TODO change true to IsDPIAware
			::MoveWindow(node.m_Hwnd,
				currentLeft * scaling,
				currentTop * scaling,
				(currentRight - currentLeft + 1) * scaling,
				(currentBottom - currentTop + 1) * scaling,
				1);

			currentTop = currentBottom;
		}

		currentLeft = currentRight;
	}

	if(i < colCount)
	{
		currentRight = m_MonitorRect.right;
		goto colHandle;
	}
}

bool WindowGrid::MoveFocus(EGridDirection direction, bool bWrapAround)
{
	bool bChanged = m_Grid.Move(direction, bWrapAround);
	if(bChanged)
	{
		Node *pNode = m_Grid.GetCurrent();
		return pNode && FocusWindow(pNode->m_Hwnd);
	}

	return bChanged;
}

bool WindowGrid::MoveWindow(EGridDirection direction, bool bWrapAround)
{
	assert(0 <= direction && direction < EGD_COUNT);

	if(m_Grid.ColumnCount() == 0){ return false; }

	// If move is horizontal and window is not alone in the column,
	// place into its own column
	if((direction & 0x1) && !m_Grid.IsLoneNode())
	{
		auto col = m_Grid.GetColumnIndex();
		size_t oldCol = col + (direction == EGD_LEFT);
		size_t newCol = col + (direction == EGD_RIGHT);
		if(!InsertColumn(newCol))
		{
			return false;
		}

		// Move window to new column
		HWND hwnd = m_Grid[oldCol][m_Grid.GetRowIndex()].m_Hwnd;
		m_Grid.InsertElement(newCol, 0, {hwnd, 1});
		m_Grid[oldCol].m_TotalHeightWeight -= m_Grid[col+1][m_Grid.GetRowIndex()].m_HeightWeight;
		m_Grid.RemoveElement(oldCol, m_Grid.GetRowIndex());
		m_Grid[newCol].m_TotalHeightWeight = 1;

		// Ensure that the current window is the one we moved
		m_Grid.SetColumnIndex(newCol);
		m_Grid.SetRowIndex(0);

		Apply();
		return true;
	}

	size_t prevCol = m_Grid.GetColumnIndex();
	size_t prevRow = m_Grid.GetRowIndex();
	HWND hwnd = m_Grid[prevCol][0].m_Hwnd;
	bool bChanged = m_Grid.Move(direction, bWrapAround);
	if(bChanged)
	{
		if(direction & 0x1) // horizontal move
		{
			size_t currCol = m_Grid.GetColumnIndex();
			m_Grid.InsertElement(currCol, m_Grid.GetRowIndex(), {hwnd, 1});
			m_Grid[currCol].m_TotalHeightWeight += 1;

			// By getting here, we know the window is alone in the column,
			// so we must get rid of the column
			RemoveColumn(prevCol);

			// Ensure that the current window is the one we moved
			m_Grid.SetColumnIndex(currCol - (prevCol < currCol));
		}
		else // vertical move
		{
			// By bChanged being true, we know that there are multiple
			// windows in this column
			auto &col = m_Grid[prevCol];
			std::swap(col[prevRow], col[m_Grid.GetRowIndex()]);
		}

		Apply();
		return true;
	}

	return bChanged;
}

void WindowGrid::Clear()
{
	m_Grid.Clear();
	m_TotalWidthWeight = 0;
}

bool WindowGrid::FocusWindow(HWND hwnd)
{
	// Move cursor into window
	RECT r;
	GetWindowRect(hwnd, &r);
	ClipCursor(&r);
	SetCursorPos(r.left + (r.right-r.left)/2, r.top + (r.bottom-r.top)/2);

	// Make hwnd the active window
	DWORD currentThreadId = GetCurrentThreadId();
	DWORD newThreadId = GetWindowThreadProcessId(hwnd, NULL);
	if(!newThreadId) return false;
	if(newThreadId != currentThreadId)
	{
		AttachThreadInput(currentThreadId, newThreadId, TRUE);
	}

	SetActiveWindow(hwnd);

	if(newThreadId != currentThreadId)
	{
		AttachThreadInput(currentThreadId, newThreadId, FALSE);
	}

	return true;
}
