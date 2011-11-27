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

import greendroid.app.GDActivity;
import greendroid.widget.ActionBar;
import android.app.Activity;
import android.app.AlarmManager;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.Toast;

public class Settings extends GDActivity {
	private long trackerRowId;
	private String trackerUrl;
	private String trackerUsername;
	private String trackerPassword;
	private boolean updateChecked;
	private long updateFrequency;
	private boolean onlyOverWifi;
	private int updatePos;
	private DBHelper dbHelper;
	private Spinner unitSpinner;
	private CheckBox updateCheckBox;
	private CheckBox wifiCheckBox;

	private class VersionHandler extends Handler {
		final ProgressDialog progressDialog;

		public VersionHandler(ProgressDialog pd) {
			progressDialog = pd;
		}

		@Override
		public void handleMessage(Message msg) {
			Bundle b = msg.getData();
			if (b.getBoolean("error")) {
				progressDialog.dismiss();
				Toast toast = Toast.makeText(getApplicationContext(),
						b.getString("errormsg"), Toast.LENGTH_LONG);
				toast.show();
			} else {
				Intent i = getIntent();

				trackerUrl = b.getString("tracker_url");
				trackerUsername = b.getString("tracker_username");
				trackerPassword = b.getString("tracker_password");

				final Spinner unitSpinner = (Spinner) findViewById(R.id.updateUnit);
				int unitPos = unitSpinner.getSelectedItemPosition();
				if (unitPos == 0)
					b.putLong("update_frequency",
							AlarmManager.INTERVAL_FIFTEEN_MINUTES);
				else if (unitPos == 1)
					b.putLong("update_frequency",
							AlarmManager.INTERVAL_HALF_HOUR);
				else if (unitPos == 2)
					b.putLong("update_frequency", AlarmManager.INTERVAL_HOUR);
				else if (unitPos == 3)
					b.putLong("update_frequency",
							AlarmManager.INTERVAL_HALF_DAY);
				else if (unitPos == 4)
					b.putLong("update_frequency", AlarmManager.INTERVAL_DAY);

				final CheckBox updateCheckBox = (CheckBox) findViewById(R.id.updateCheckbox);
				b.putBoolean("update_checked", updateCheckBox.isChecked());
				b.putBoolean("only_wifi", wifiCheckBox.isChecked());
				b.putBoolean("tracker_changed", true);

				if (trackerRowId == -1) {
					trackerRowId = dbHelper.insertTracker(trackerUrl,
							trackerUsername, trackerPassword);
					b.putLong("tracker_id", trackerRowId);
				} else {
					dbHelper.updateTracker(trackerRowId, trackerUrl,
							trackerUsername, trackerPassword);
				}

				dbHelper.insertFields(trackerRowId, "component",
						b.getStringArray("tracker_components"));
				dbHelper.insertFields(trackerRowId, "milestone",
						b.getStringArray("tracker_milestones"));
				dbHelper.insertFields(trackerRowId, "priority",
						b.getStringArray("tracker_priorities"));
				dbHelper.insertFields(trackerRowId, "resolution",
						b.getStringArray("tracker_resolutions"));
				dbHelper.insertFields(trackerRowId, "severity",
						b.getStringArray("tracker_severities"));
				dbHelper.insertFields(trackerRowId, "status",
						b.getStringArray("tracker_statuses"));
				dbHelper.insertFields(trackerRowId, "version",
						b.getStringArray("tracker_versions"));
				i.putExtras(b);
				setResult(Activity.RESULT_OK, i);
				finish();
				progressDialog.dismiss();
			}

		}
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.settings);
		getActionBar().setType(ActionBar.Type.Empty);
		setTitle("Entomologist Settings");
		Bundle extras = getIntent().getExtras();
		dbHelper = new DBHelper(this);
		trackerRowId = extras.getLong("tracker_id", -1);
		trackerUrl = extras.getString("tracker_url");
		trackerUsername = extras.getString("tracker_username");
		trackerPassword = extras.getString("tracker_password");
		updateFrequency = extras.getLong("update_frequency");
		updateChecked = extras.getBoolean("update_checked", false);
		onlyOverWifi = extras.getBoolean("only_wifi", true);

		final EditText urlBox = (EditText) findViewById(R.id.urlEdit);
		final EditText userBox = (EditText) findViewById(R.id.usernameEdit);
		final EditText passBox = (EditText) findViewById(R.id.passwordEdit);
		unitSpinner = (Spinner) findViewById(R.id.updateUnit);
		String[] unitArray = { "15 minutes", "30 minutes", "1 hour",
				"12 hours", "24 hours" };
		ArrayAdapter<String> spinnerArrayAdapter = new ArrayAdapter<String>(
				this, android.R.layout.simple_spinner_item, unitArray);
		unitSpinner.setAdapter(spinnerArrayAdapter);
		if (updateFrequency == AlarmManager.INTERVAL_FIFTEEN_MINUTES)
			unitSpinner.setSelection(0);
		else if (updateFrequency == AlarmManager.INTERVAL_HALF_HOUR)
			unitSpinner.setSelection(1);
		else if (updateFrequency == AlarmManager.INTERVAL_HOUR)
			unitSpinner.setSelection(2);
		else if (updateFrequency == AlarmManager.INTERVAL_HALF_DAY)
			unitSpinner.setSelection(3);
		else if (updateFrequency == AlarmManager.INTERVAL_DAY)
			unitSpinner.setSelection(4);
		else
			Log.e("Entomologist","Couldn't find a match!");
		updatePos = unitSpinner.getSelectedItemPosition();

		updateCheckBox = (CheckBox) findViewById(R.id.updateCheckbox);
		wifiCheckBox = (CheckBox) findViewById(R.id.onlyOverWifiCheckbox);
		updateCheckBox.setChecked(updateChecked);
		wifiCheckBox.setChecked(onlyOverWifi);
		unitSpinner.setEnabled(updateChecked);

		updateCheckBox
				.setOnCheckedChangeListener(new OnCheckedChangeListener() {
					@Override
					public void onCheckedChanged(CompoundButton buttonView,
							boolean isChecked) {
						unitSpinner.setEnabled(isChecked);
					}
				});
		urlBox.setText(trackerUrl);
		userBox.setText(trackerUsername);
		passBox.setText(trackerPassword);

		Button saveButton = (Button) findViewById(R.id.settingsSaveButton);
		saveButton.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				String url = urlBox.getText().toString();
				String username = userBox.getText().toString();
				String password = passBox.getText().toString();
				if (!checkSettings(url, username, password))
					return;

				// If the URL is different, delete bugs and reload everything.
				// If just the username or password are different, just verify
				// that it's correct
				if (!url.equals(trackerUrl)) {
					TracBackend backend = new TracDetailsTask(
							getApplicationContext(), url, username, password,
							false);
					backend.execute(new VersionHandler(ProgressDialog.show(
							Settings.this, "",
							"Getting tracker information...", true)));
				} else if ((!username.equals(trackerUsername))
						|| (!password.equals(trackerPassword))) {
					TracBackend backend = new TracDetailsTask(
							getApplicationContext(), trackerUrl,
							trackerUsername, trackerPassword, true);
					backend.execute(new VersionHandler(ProgressDialog.show(
							Settings.this, "",
							"Verifying username and password...", true)));
				} else if (updatesChanged()) {
					//Log.v("Entomologist", "Updates Changed");
					Intent intent = getIntent();
					intent.putExtra("tracker_changed", false);
					final Spinner unitSpinner = (Spinner) findViewById(R.id.updateUnit);
					int unitPos = unitSpinner.getSelectedItemPosition();
					if (unitPos == 0)
						intent.putExtra("update_frequency",
								AlarmManager.INTERVAL_FIFTEEN_MINUTES);
					else if (unitPos == 1)
						intent.putExtra("update_frequency",
								AlarmManager.INTERVAL_HALF_HOUR);
					else if (unitPos == 2)
						intent.putExtra("update_frequency", AlarmManager.INTERVAL_HOUR);
					else if (unitPos == 3)
						intent.putExtra("update_frequency",
								AlarmManager.INTERVAL_HALF_DAY);
					else if (unitPos == 4)
						intent.putExtra("update_frequency", AlarmManager.INTERVAL_DAY);


					intent.putExtra("update_checked",
							updateCheckBox.isChecked());
					intent.putExtra("only_wifi", wifiCheckBox.isChecked());
					setResult(Activity.RESULT_OK, intent);
					finish();
				} else {
					// Nothing has changed, so assume they meant "cancel"
					Intent i = getIntent();
					setResult(Activity.RESULT_CANCELED, i);
					finish();
				}
			}
		});

	}

	private boolean updatesChanged() {
		boolean newState = updateCheckBox.isChecked();
		if (newState != updateChecked) {
			return true;
		} else if (newState) {
			//Log.v("Entomologist", "Update checked");
			if (unitSpinner.getSelectedItemPosition() != updatePos)
				return true;

		} else if (onlyOverWifi != wifiCheckBox.isChecked()) {
			return true;
		}

		return false;
	}

	private boolean checkSettings(String url, String username, String password) {
		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setPositiveButton("OK", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				dialog.cancel();
			}
		});
		
		if (url.length() == 0) {
			builder.setMessage("The URL cannot be empty.");
		} else if (username.length() == 0) {
			builder.setMessage("The username cannot be empty.");
		} else if (password.length() == 0) {
			builder.setMessage("The password cannot be empty.");
		} else {
			return true;
		}

		AlertDialog alert = builder.create();
		alert.show();
		return false;
	}
	
	@Override
	public void onDestroy()
	{
		super.onDestroy();
		dbHelper.close();
	}
}
