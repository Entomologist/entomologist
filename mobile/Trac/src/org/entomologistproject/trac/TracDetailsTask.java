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

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import org.apache.http.conn.HttpHostConnectException;
import org.xmlrpc.android.*;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

// This AsyncTask checks the API for validity, and downloads the list of possible field values
public class TracDetailsTask extends TracBackend {
	final boolean versionCheckOnly;
	public TracDetailsTask(Context context, 
						   String url,
						   String username,
						   String password,
						   boolean justCheckVersion)
	{
		super(context, url, username, password);
		versionCheckOnly = justCheckVersion;
	}
	
	public String getVersion() throws EntomologistException
	{
		//Log.v("Entomologist", "getVersion()");
		String ret = "0";
		if (service.length() == 0)
			return ret;
		try 
		{
		  Object[] array = (Object[]) client.call("system.getAPIVersion");
		  //Log.v("Entomologist", "client.call finished for getVersion");
			  // Epoch.  0 = 0.10, 1 = 0.11 & 0.12
		  if (((Integer) array[0] == 0) || ((Integer) array[1] != 1))
		  {
			  throw new EntomologistException("The version is too low");
		  }
		  ret = "0." + (Integer) array[1] + (Integer) array[2];
		}
		catch (XMLRPCException e)
		{
			//Log.v("Entomologist", "XMLRPCException");
		  e.printStackTrace();
	      throw new EntomologistException(e.getMessage());
		}
		return ret;
	}
	
	public String[] getComponents() throws EntomologistException
	{
		return doFieldList("ticket.component.getAll");
	}
	
	public String[] getMilestones() throws EntomologistException
	{
		// The milestone can be blank
		String[] milestones = doFieldList("ticket.milestone.getAll");
		if (milestones != null)
		{
			String[] ret = new String[milestones.length + 1];
			ret[0] = new String("");
			System.arraycopy(milestones, 0, ret, 1, milestones.length);
			return ret;
		}
		else
		{
			return null;
		}
	}
	public String[] getPriorities() throws EntomologistException
	{
		return doFieldList("ticket.priority.getAll");
	}
	public String[] getResolutions() throws EntomologistException
	{
		return doFieldList("ticket.resolution.getAll");
	}
	public String[] getSeverities() throws EntomologistException
	{
		// "type" and "severity" mean the same thing in trac,
		// and users can use both.  So make both calls and merge them		
		String[] severities = doFieldList("ticket.severity.getAll");
		String[] types = doFieldList("ticket.type.getAll");
		Set<String> set = new HashSet<String>();
		if (severities != null)
			set.addAll(Arrays.asList(severities));
		if (types != null)
			set.addAll(Arrays.asList(types));
		final String[] ret = new String[set.size()];
		set.toArray(ret);
		return ret;
	}
	
	public String[] getStatuses() throws EntomologistException
	{
		return doFieldList("ticket.status.getAll");
	}

	public String[] getVersions() throws EntomologistException
	{
		// The version can be blank
		String[] versions = doFieldList("ticket.version.getAll");
		if (versions != null)
		{
			String[] ret = new String[versions.length + 1];
			ret[0] = new String("");
			System.arraycopy(versions, 0, ret, 1, versions.length);
			return ret;
		}
		else
		{
			return null;
		}
	}
	
	public String[] doFieldList(String call) throws EntomologistException
	{
		String[] items = null;
		Object[] array;
		try
		{
			array = (Object[]) client.call(call);
		}
		catch (XMLRPCException e)
		{
			e.printStackTrace();
			throw new EntomologistException(e.getMessage());
		}
		
		if (array.length == 0)
			return items;
		
		items = new String[array.length];
		for (int i = 0; i < array.length; ++i)
		{
			items[i] = (String) array[i];
		}
		
		return items;
	}
	/*
	public void doMultiCall() throws EntomologistException
	{
		Object[] array;
		try
		{
			HashMap<String, Object> component = new HashMap<String, Object>();
			component.put("methodName", "ticket.component.getAll");
			component.put("params", new Object[0]);
			HashMap<String, Object> priority = new HashMap<String, Object>();
			component.put("methodName", "ticket.priority.getAll");
			component.put("params", new Object[0]);
			Object[] paramArray = new Object[2];
			paramArray[0] = component;
			paramArray[1] = priority;
			array = (Object[]) client.call("system.multicall", paramArray);
		}
		catch (XMLRPCException e)
		{
			e.printStackTrace();
			throw new EntomologistException(e.getMessage());
		}
		
		if (array.length > 0)
		{
			Object[] array1 = (Object[]) array[0];
			if (array1.length > 0)
			{
				Object[] array2 = (Object[]) array1[0];
				if (array2.length > 0)
					Log.v("Entomologist", (String) array2[0]);
			}
		}
	}
	*/
	@Override
	protected Void doInBackground(Handler... params) {
		Handler h = params[0];
		Message m = h.obtainMessage();
		Bundle b = new Bundle();
		b.putBoolean("error", false);
		try
		{
			checkHost();
			String s = this.getVersion();			
			b.putString("tracker_version", s);
			if (!versionCheckOnly)
			{
				// This is very inefficient, but the android xmlrpc library
				// seems to reverse the order of arrays, so we can't safely use Trac's system.multicall()
				// for now.
				//Log.v("Entomologist", "Calling getComponents()");
				String[] components = this.getComponents();
				if (components != null)
					b.putStringArray("tracker_components", components);
				String[] milestones = this.getMilestones();
				if (milestones != null)
					b.putStringArray("tracker_milestones", milestones);
	
				String[] priorities = this.getPriorities();
				if (priorities != null)
					b.putStringArray("tracker_priorities", priorities);
	
				String[] resolutions = this.getResolutions();
				if (resolutions != null)
					b.putStringArray("tracker_resolutions", resolutions);
	
				String[] severities = this.getSeverities();
				if (severities != null)
					b.putStringArray("tracker_severities", severities);
				
				String[] statuses = this.getStatuses();
				if (statuses != null)
					b.putStringArray("tracker_statuses", statuses);
	

				String[] versions = this.getVersions();
				if (versions != null)
					b.putStringArray("tracker_versions", versions);
				b.putString("tracker_url", url);
				b.putString("tracker_username", username);
				b.putString("tracker_password", password);
			}
		}
		catch (EntomologistException e)
		{
			Log.e("Entomologist", "EntomologistException caught: " + e.getMessage());
			b.putBoolean("error", true);
			b.putString("errormsg", e.getMessage());
		}
		
		m.setData(b);
		m.sendToTarget();
		dbHelper.close();

		return null;
	}


}
