/***************************************************************************
                          milkywaycomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/08/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MILKYWAYCOMPONENT_H
#define MILKYWAYCOMPONENT_H

#include "linelistcomponent.h"

#include <QList>


class KStarsData;

/**
	*@class MilkyWayComponent
	*Represents the milky way contour.
	
	*@author Thomas Kabelmann
	*@version 0.1
	*/
class MilkyWayComponent : public LineListComponent
{
	public:
		
		/**
			*@short Constructor
			*@p parent pointer to the parent SkyComponent
			*/
		MilkyWayComponent(SkyComponent *parent, bool (*visibleMethod)());

		/**
			*@short Destructor
			*/
		~MilkyWayComponent();

		/**
			*@short Draw the Milky Way on the sky map
			*@p ks Pointer to the KStars object
			*@p psky Reference to the QPainter on which to paint
			*@p scale the scaling factor for drawing (1.0 for screen draws)
			*/
		virtual void draw(KStars *ks, QPainter& psky, double scale);

		/**
		 *@short Draw the Milky Way in integer mode
		 */
		void drawInt(KStars *ks, QPainter& psky, double scale);
        
		/**
		 *@short Draw the Milky Way in antialiased mode
		 **/
		void drawFloat(KStars *ks, QPainter& psky, double scale);

		/**
			*@short Initialize the Milky Way
			*
			*The lines in each file are parsed according to column position:
			*@li 0-7     RA [float]
			*@li 9-16    Dec [float]
			*@p data Pointer to the KStarsData object
			*/
		virtual void init(KStarsData *data);
	
		void addPoint( const SkyPoint &p );
		
		QHash<int, bool> skip;
 private:
		SkyPoint *FirstPoint;
};

#endif
