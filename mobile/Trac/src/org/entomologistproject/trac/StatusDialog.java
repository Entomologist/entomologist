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

import android.app.Dialog;
import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;

public class StatusDialog extends Dialog  {
	private String[] statuses;
	private String[] resolutions;
	private TextView resolutionText;
	private TextView detailsStatusView;
	private TextView detailsResolutionView;
	private Spinner statusSpinner;
	private Spinner resolutionSpinner;
	private int closedIndex;
	private String currentStatus;
	private String currentResolution;
	
	public StatusDialog(Context context,
						String[] statuses,
						String[] resolutions,
						TextView status,
						TextView resolution) {
		super(context);
		this.statuses = statuses;
		this.resolutions = resolutions;
		this.closedIndex = -1;
		this.detailsStatusView = status;
		this.detailsResolutionView = resolution;
		this.currentStatus = status.getText().toString();
		this.currentResolution = resolution.getText().toString();
	}
	
	public String getStatus()
	{
		int index = statusSpinner.getSelectedItemPosition();
		return statuses[index];
	}
	
	public String getResolution()
	{
		int index = resolutionSpinner.getSelectedItemPosition();
		return resolutions[index];
	}
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.status_dialog);
		setTitle("Status and Resolution");
		resolutionText = (TextView) findViewById(R.id.textView2);
		statusSpinner = (Spinner) findViewById(R.id.statusSpinner);
		resolutionSpinner = (Spinner) findViewById(R.id.resolutionSpinner);

		ArrayAdapter<String> statusAdapter = new ArrayAdapter<String>(getContext(), 
				R.layout.spinner_view, R.id.spinnerText,
				statuses);
	    
		ArrayAdapter<String> resolutionAdapter = new ArrayAdapter<String>(getContext(), 
				R.layout.spinner_view, R.id.spinnerText,
				resolutions);

		statusSpinner.setAdapter(statusAdapter);
		statusSpinner.setSelection(statusAdapter.getPosition(currentStatus));
		statusSpinner.setOnItemSelectedListener(new itemSelectedListener());
		
		resolutionSpinner.setAdapter(resolutionAdapter);
		resolutionSpinner.setSelection(resolutionAdapter.getPosition(currentResolution));
		resolutionSpinner.setOnItemSelectedListener(new itemSelectedListener());
		int i;
		for (i = 0; i < statuses.length; ++i)
		{
			if (((String) statuses[i]).equals("closed"))
				closedIndex = i;
		}

		toggleResolution();
		Button b = (Button) findViewById(R.id.statusDialogOkButton);
		b.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				String currentStatus = getStatus();
				detailsStatusView.setText(getStatus());
				if (currentStatus.equals("closed"))
						detailsResolutionView.setText(getResolution());
				else
					detailsResolutionView.setText("");
				dismiss();
			}
			
		});
	}

	public void toggleResolution()
	{
		if (statusSpinner.getSelectedItemPosition() == closedIndex)
		{
			resolutionText.setEnabled(true);
			resolutionSpinner.setEnabled(true);
		}
		else
		{
			resolutionText.setEnabled(false);
			resolutionSpinner.setEnabled(false);
		}
	}
	private class itemSelectedListener implements OnItemSelectedListener {

	    public void onItemSelected(AdapterView<?> parent,
	        View view, int pos, long id) {
	    	toggleResolution();
	    }

	    @SuppressWarnings("rawtypes")
		public void onNothingSelected(AdapterView parent) {
	      // Do nothing.
	    }
	}
}