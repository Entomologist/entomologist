/*
 *  Entomologist Mobile/Trac
 *  Copyright (C) 2011 Matt Barringer
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 *  Author: Matt Barringer <matt@entomologist-project.org>
 */

package org.entomologistproject.trac;

import android.content.Context;
import android.database.Cursor;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.SimpleCursorAdapter;

public class AlternateRowCursorAdapter extends SimpleCursorAdapter {
	private static int[] colors = new int[] { Color.parseColor("#80a8c24e"), Color.parseColor("#80F0F0F0") };
	
	public AlternateRowCursorAdapter(Context context, int layout, Cursor c,
			String[] from, int[] to) {
		super(context, layout, c, from, to);
	}
	
	@Override  
	public View getView(int position, View convertView, ViewGroup parent)
	{  
		View view = super.getView(position, convertView, parent);  
		int colorPos = position % colors.length;  
		view.setBackgroundColor(colors[colorPos]);  
		return view;  
	}  

}
