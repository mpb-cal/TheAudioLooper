
#include "Section.h"

const double MEDIA_TIME_DIVISOR = 10000000.0;

Section::Section()
{
	setSecondsPerBeat( 1 );
	
	setStart( 0 );
	setStop( 1 );
}

void Section::setSecondsPerBeat( double t )
{
	if (t < .01 || t > 10) return;

	m_secondsPerBeat = t;
	m_beatsPerMinute = 60.0 / m_secondsPerBeat;
}

void Section::setBeatsPerMinute( double t )
{
	if (t < 1 || t > 10000) return;

	m_beatsPerMinute = t;
	m_secondsPerBeat = 60.0 / m_beatsPerMinute;
}

void Section::setStart( __int64 t )
{
	if (t < 0 || t >= m_stopPos || t > m_duration) return;

	m_startPos = t;
	m_length = m_stopPos - m_startPos;
}

void Section::setStop( __int64 t )
{
	if (t < 0 || t <= m_startPos || t > m_duration) return;

	m_stopPos = t;
	m_length = m_stopPos - m_startPos;
}

void Section::setLength( __int64 t )
{
	if (t <= 0 || t > m_duration) return;

	m_length = t;
	m_stopPos = m_startPos + m_length;
}

void Section::setDuration( __int64 t )
{
	if (t <= 0) return;

	m_duration = t;
}

void Section::setStartInSeconds( double s )
{
	setStart( (__int64)(s * MEDIA_TIME_DIVISOR) );
}

void Section::setStopInSeconds( double s )
{
	setStop( (__int64)(s * MEDIA_TIME_DIVISOR) );
}

void Section::setLengthInSeconds( double s )
{
	setLength( (__int64)(s * MEDIA_TIME_DIVISOR) );
}

void Section::setStartInBeats( double b )
{
	setStart( (__int64)(b * m_secondsPerBeat * MEDIA_TIME_DIVISOR) );
}

void Section::setStopInBeats( double b )
{
	setStop( (__int64)(b * m_secondsPerBeat * MEDIA_TIME_DIVISOR) );
}

void Section::setLengthInBeats( double b )
{
	setLength( (__int64)(b * m_secondsPerBeat * MEDIA_TIME_DIVISOR) );
}

void Section::moveStartInSeconds( int dir, double seconds )
{
	if (dir == MOVE_LEFT) setStartInSeconds( getStartInSeconds() - seconds );
	else setStartInSeconds( getStartInSeconds() + seconds );
}

void Section::moveStopInSeconds( int dir, double seconds )
{
	if (dir == MOVE_LEFT) setStopInSeconds( getStopInSeconds() - seconds );
	else setStopInSeconds( getStopInSeconds() + seconds );
}

double Section::getSecondsPerBeat()
{
	return m_secondsPerBeat;
}

double Section::getBeatsPerMinute()
{
	return m_beatsPerMinute;
}

__int64 Section::getStart()
{
	return m_startPos;
}

__int64 Section::getStop()
{
	return m_stopPos;
}

__int64 Section::getLength()
{
	return m_length;
}

__int64 Section::getDuration()
{
	return m_duration;
}

double Section::getStartInSeconds()
{
	return (double)(m_startPos / MEDIA_TIME_DIVISOR);
}

double Section::getStopInSeconds()
{
	return (double)(m_stopPos / MEDIA_TIME_DIVISOR);
}

double Section::getLengthInSeconds()
{
	return (double)(m_length / MEDIA_TIME_DIVISOR);
}


double Section::getStartInBeats()
{
	return getStartInSeconds() / m_secondsPerBeat;
}

double Section::getStopInBeats()
{
	return getLengthInSeconds() / m_secondsPerBeat;
}

double Section::getLengthInBeats()
{
	return getLengthInSeconds() / m_secondsPerBeat;
}


