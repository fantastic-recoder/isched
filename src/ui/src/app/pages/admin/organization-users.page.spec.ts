import { TestBed } from '@angular/core/testing';
import { UsersPage } from './users.page';

describe('UsersPage org context guard', () => {
  it('blocks organization switch while dirty form guard is active', async () => {
    await TestBed.configureTestingModule({ imports: [UsersPage] }).compileComponents();
    const fixture = TestBed.createComponent(UsersPage);
    const component = fixture.componentInstance;

    component.markDirty();
    component.onOrgChange('org-b');

    expect(component.warning()).toContain('unsaved changes');
  });
});

