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
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.provider.BaseColumns;

public class EntomologistDatabase extends SQLiteOpenHelper {
	private static final int DATABASE_VERSION = 1;
	private static final String trackerTableCreate = "CREATE TABLE trackers ("
													 + BaseColumns._ID + " INTEGER PRIMARY KEY AUTOINCREMENT, "
		                                             +"name VARCHAR, url VARCHAR, username VARCHAR, password VARCHAR, "
		                                             + "last_updated VARCHAR, version VARCHAR)";
	private static final String savedTableCreate = "CREATE TABLE saved_bugs ("
													+ BaseColumns._ID + " INTEGER PRIMARY KEY AUTOINCREMENT, "
													+ "tracker_id INTEGER, "
													+ "bug_id INTEGER)";
	private static final String attachmentTableCreate = "CREATE TABLE attachments ("
													+ BaseColumns._ID + " INTEGER PRIMARY KEY AUTOINCREMENT, "
													+ "tracker_id INTEGER, "
													+ "bug_id INTEGER, "
													+ "filename VARCHAR, "
													+ "description TEXT, "
													+ "size INTEGER, "
													+ "timestamp VARCHAR, "
													+ "submitter TEXT)";
	
	private static final String bugsTableCreate = "CREATE TABLE bugs ("
												  + BaseColumns._ID + " INTEGER PRIMARY KEY AUTOINCREMENT, "
												  + "tracker_id INTEGER, "
												  + "bug_id INTEGER, "
												  + "reporter VARCHAR, "
												  + "severity VARCHAR, "
												  + "priority VARCHAR, "
												  + "assigned_to VARCHAR, "
												  + "status VARCHAR, "
												  + "summary TEXT, "
												  + "component VARCHAR, "
												  + "milestone VARCHAR, "
												  + "version VARCHAR, " 
												  + "resolution VARCHAR, "
												  + "bug_type VARCHAR, "
												  + "description TEXT, "
												  + "ui_notice TEXT, "
												  + "last_modified VARCHAR)";
	
	private static final String commentsTableCreate = "CREATE TABLE comments ("
													 + BaseColumns._ID + " INTEGER PRIMARY KEY AUTOINCREMENT, "
													 + "tracker_id INTEGER, "
													 + "bug_id INTEGER, "
													 + "comment_id INTEGER, "
													 + "author TEXT, "
													 + "comment TEXT, "
													 + "timestamp TEXT)";
	private static final String fieldsTableCreate = "CREATE TABLE fields ("
													+ BaseColumns._ID + " INTEGER PRIMARY KEY AUTOINCREMENT, "
													+ "tracker_id INTEGER, "
													+ "field_name VARCHAR, "
													+ "value VARCHAR)";
	
	EntomologistDatabase(Context context) {
        super(context, "Entomologist", null, DATABASE_VERSION);
    }

	@Override
	public void onCreate(SQLiteDatabase db) {
		db.execSQL(trackerTableCreate);
		db.execSQL(attachmentTableCreate);
		db.execSQL(savedTableCreate);
		db.execSQL(bugsTableCreate);
		db.execSQL(commentsTableCreate);
		db.execSQL(fieldsTableCreate);
	}

	@Override
	public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
		// TODO Auto-generated method stub
	}

}
